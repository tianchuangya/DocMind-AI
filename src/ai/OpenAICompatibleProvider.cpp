// DocMind AI — OpenAICompatibleProvider 实现
//
// QNetworkAccessManager 异步发起 chat/embeddings；
// chat stream=true 时按 SSE 解析逐 chunk 推送信号。
#include "ai/OpenAICompatibleProvider.h"
#include "utils/Logger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QPointer>
#include <QFutureInterface>
#include <QRegularExpression>

namespace dmc::ai {

namespace {

// 规范化 baseUrl：保证以 /v1 结尾，处理用户填法
QString normalizeBaseUrl(const QString& in) {
    QString s = in.trimmed();
    while (s.endsWith(QLatin1Char('/'))) s.chop(1);
    if (s.isEmpty()) return s;
    if (s.endsWith(QStringLiteral("/v1"))) return s;
    return s + QStringLiteral("/v1");
}

QByteArray buildChatBody(const ChatRequest& req) {
    QJsonObject body;
    body[QStringLiteral("model")]       = req.model;
    body[QStringLiteral("temperature")]  = double(req.temperature);
    body[QStringLiteral("stream")]       = req.stream;
    if (req.maxTokens) body[QStringLiteral("max_tokens")] = *req.maxTokens;
    if (req.topP < 1.0f) body[QStringLiteral("top_p")] = double(req.topP);
    if (!req.stopSequences.isEmpty()) {
        QJsonArray arr;
        for (const QString& s : req.stopSequences) arr.append(s);
        body[QStringLiteral("stop")] = arr;
    }

    QJsonArray messages;
    for (const ChatMessage& m : req.messages) {
        QJsonObject o;
        o[QStringLiteral("role")]    = QString::fromLatin1(roleToString(m.role));
        o[QStringLiteral("content")] = m.content;
        messages.append(o);
    }
    body[QStringLiteral("messages")] = messages;
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray buildEmbeddingBody(const EmbeddingRequest& req) {
    QJsonObject body;
    body[QStringLiteral("model")] = req.model;
    QJsonArray inputs;
    for (const QString& s : req.inputs) inputs.append(s);
    body[QStringLiteral("input")] = inputs;
    if (!req.encodingFormat.isEmpty())
        body[QStringLiteral("encoding_format")] = req.encodingFormat;
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

ChatResult parseChatResult(const QJsonDocument& doc) {
    ChatResult r;
    QJsonObject root = doc.object();
    QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        QJsonObject choice = choices.at(0).toObject();
        QJsonObject msg = choice.value(QStringLiteral("message")).toObject();
        r.content      = msg.value(QStringLiteral("content")).toString();
        r.finishReason = choice.value(QStringLiteral("finish_reason")).toString();
    }
    QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
    r.usage.promptTokens     = usage.value(QStringLiteral("prompt_tokens")).toInt();
    r.usage.completionTokens= usage.value(QStringLiteral("completion_tokens")).toInt();
    r.usage.totalTokens      = usage.value(QStringLiteral("total_tokens")).toInt();
    r.model      = root.value(QStringLiteral("model")).toString();
    r.rawResponse = root;
    return r;
}

// 解析一行 SSE 内容（已去掉 "data: " 前缀）
// 返回 nullopt 表示 [DONE] 或非 JSON 行
std::optional<QJsonObject> parseSseData(const QByteArray& line) {
    QByteArray data = line.trimmed();
    if (data.isEmpty()) return std::nullopt;
    if (data == "[DONE]") return std::nullopt;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return std::nullopt;
    return doc.object();
}

} // namespace

OpenAICompatibleProvider::OpenAICompatibleProvider(QObject* parent)
    : AIProvider(parent), m_nam(new QNetworkAccessManager(this)) {}

OpenAICompatibleProvider::~OpenAICompatibleProvider() = default;

void OpenAICompatibleProvider::applyConfig(const ProviderConfig& cfg) {
    m_config = cfg;
    LOG_INFO("OpenAICompatibleProvider",
             QString("Configured base=%1 chat=%2 emb=%3")
                 .arg(cfg.baseUrl, cfg.chatModel, cfg.embeddingModel));
}

QString OpenAICompatibleProvider::chatUrl() const {
    return normalizeBaseUrl(m_config.baseUrl) + QStringLiteral("/chat/completions");
}

QString OpenAICompatibleProvider::embeddingsUrl() const {
    return normalizeBaseUrl(m_config.baseUrl) + QStringLiteral("/embeddings");
}

AIErrorInfo OpenAICompatibleProvider::mapError(int httpStatus, const QByteArray& body,
                                                  const QVariant& userTag) const {
    AIErrorInfo info;
    info.httpStatus = httpStatus;
    info.userTag    = userTag;
    info.message    = QString::fromUtf8(body);

    // 试图解析供应商错误体
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error == QJsonParseError::NoError) {
        QJsonObject obj = doc.object();
        QJsonObject errObj = obj.value(QStringLiteral("error")).toObject();
        info.providerCode = errObj.value(QStringLiteral("code")).toString();
        QString msg = errObj.value(QStringLiteral("message")).toString();
        if (!msg.isEmpty()) info.message = msg;
    }

    if (httpStatus == 0)            info.code = AIError::NetworkError;
    else if (httpStatus == 401 || httpStatus == 403) info.code = AIError::AuthFailed;
    else if (httpStatus == 404)     info.code = AIError::ModelUnavailable;
    else if (httpStatus == 429)     info.code = AIError::RateLimited;
    else if (httpStatus >= 500)     info.code = AIError::ServerError;
    else if (httpStatus >= 400)     info.code = AIError::InvalidRequest;

    // 脱敏：屏蔽任何可能的 Bearer token 出现（实际不会出现，因为请求体不含 key）
    info.message.remove(QRegularExpression(QStringLiteral("sk-[A-Za-z0-9]{10,}"),
                                              QRegularExpression::CaseInsensitiveOption));
    return info;
}

std::optional<QJsonObject> OpenAICompatibleProvider::parseSseLine(const QByteArray& line) {
    if (!line.startsWith("data:")) return std::nullopt;
    QByteArray data = line.mid(5).trimmed();
    return parseSseData(data);
}

// ─── 非流式 chat ────────────────────────────────────────────────────────────

QFuture<ChatResult> OpenAICompatibleProvider::chat(const ChatRequest& req) {
    auto iface = std::make_shared<QFutureInterface<ChatResult>>();
    iface->reportStarted();

    ChatRequest r = req;
    r.stream = false;
    if (r.model.isEmpty()) r.model = m_config.chatModel;

    QNetworkRequest nr((QUrl(chatUrl())));
    nr.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    nr.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.apiKey).toUtf8());

    int timeout = r.timeoutMs.value_or(m_config.requestTimeoutMs);
    QPointer<QNetworkReply> reply = m_nam->post(nr, buildChatBody(r));

    auto timer = new QTimer(reply.data());
    timer->setInterval(timeout);
    timer->setSingleShot(true);

    QObject::connect(timer, &QTimer::timeout, reply.data(), [this, reply, iface, r]() {
        if (!reply) return;
        AIErrorInfo err;
        err.code = AIError::Timeout;
        err.message = QStringLiteral("Request timed out after %1 ms").arg(r.timeoutMs.value_or(m_config.requestTimeoutMs));
        err.userTag = r.userTag;
        LOG_WARN("OpenAICompatibleProvider", err.message);
        reply->abort();
        emit requestFailed(err);
        iface->reportException(std::runtime_error(err.message.toStdString()));
        iface->reportFinished();
    });

    QObject::connect(reply.data(), &QNetworkReply::finished, reply.data(), [this, reply, iface, r, timer]() {
        timer->stop();
        if (!reply) {
            iface->reportFinished();
            return;
        }
        reply->deleteLater();

        int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        QNetworkReply::NetworkError netErr = reply->error();

        if (netErr != QNetworkReply::NoError && http == 0) {
            AIErrorInfo err;
            err.code = (netErr == QNetworkReply::OperationCanceledError)
                       ? AIError::Cancelled : AIError::NetworkError;
            err.message = reply->errorString();
            err.userTag = r.userTag;
            emit requestFailed(err);
            iface->reportException(std::runtime_error(err.message.toStdString()));
            iface->reportFinished();
            return;
        }
        if (http >= 400) {
            emit requestFailed(mapError(http, body, r.userTag));
            iface->reportException(std::runtime_error("HTTP " + std::to_string(http)));
            iface->reportFinished();
            return;
        }
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
        if (pe.error != QJsonParseError::NoError) {
            AIErrorInfo err;
            err.code = AIError::ResponseFormatInvalid;
            err.message = QStringLiteral("Invalid JSON response");
            err.userTag = r.userTag;
            emit requestFailed(err);
            iface->reportException(std::runtime_error(err.message.toStdString()));
            iface->reportFinished();
            return;
        }
        iface->reportResult(parseChatResult(doc));
        iface->reportFinished();
    });

    timer->start();
    return iface->future();
}

// ─── 流式 chat ──────────────────────────────────────────────────────────────

QFuture<ChatResult> OpenAICompatibleProvider::chatStream(const ChatRequest& req) {
    auto iface = std::make_shared<QFutureInterface<ChatResult>>();
    iface->reportStarted();

    ChatRequest r = req;
    r.stream = true;
    if (r.model.isEmpty()) r.model = m_config.chatModel;

    QNetworkRequest nr((QUrl(chatUrl())));
    nr.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    nr.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.apiKey).toUtf8());
    nr.setRawHeader("Accept", "text/event-stream");

    QPointer<QNetworkReply> reply = m_nam->post(nr, buildChatBody(r));

    // 聚合流式 token
    auto acc = std::make_shared<QString>();
    auto usage= std::make_shared<ChatUsage>();
    auto finish = std::make_shared<QString>();

    QObject::connect(reply.data(), &QNetworkReply::readyRead, reply.data(),
        [this, reply, acc, usage, finish, r]() {
            if (!reply) return;
            while (reply->canReadLine()) {
                QByteArray line = reply->readLine();
                auto obj = parseSseLine(line);
                if (!obj) continue;

                QJsonArray choices = obj->value(QStringLiteral("choices")).toArray();
                if (!choices.isEmpty()) {
                    QJsonObject choice = choices.at(0).toObject();
                    QJsonObject delta = choice.value(QStringLiteral("delta")).toObject();
                    QString piece = delta.value(QStringLiteral("content")).toString();
                    if (!piece.isEmpty()) {
                        acc->append(piece);
                        ChatStreamEvent ev;
                        ev.type  = ChatStreamEvent::Chunk;
                        ev.delta = piece;
                        emit chatStreamChunk(ev, r.userTag);
                    }
                    QString fr = choice.value(QStringLiteral("finish_reason")).toString();
                    if (!fr.isEmpty()) *finish = fr;
                }
                QJsonObject u = obj->value(QStringLiteral("usage")).toObject();
                if (!u.isEmpty()) {
                    usage->promptTokens     = u.value(QStringLiteral("prompt_tokens")).toInt();
                    usage->completionTokens = u.value(QStringLiteral("completion_tokens")).toInt();
                    usage->totalTokens      = u.value(QStringLiteral("total_tokens")).toInt();
                    ChatStreamEvent ev; ev.type = ChatStreamEvent::Usage; ev.usage = *usage;
                    emit chatStreamChunk(ev, r.userTag);
                }
            }
        });

    QObject::connect(reply.data(), &QNetworkReply::finished, reply.data(),
        [this, reply, acc, usage, finish, iface, r]() {
            if (!reply) { iface->reportFinished(); return; }
            reply->deleteLater();

            int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (http >= 400) {
                emit requestFailed(mapError(http, reply->readAll(), r.userTag));
                iface->reportException(std::runtime_error("HTTP " + std::to_string(http)));
                iface->reportFinished();
                return;
            }
            ChatStreamEvent done;
            done.type = ChatStreamEvent::Done;
            done.finishReason = finish->isEmpty() ? QStringLiteral("stop") : *finish;
            emit chatStreamChunk(done, r.userTag);

            ChatResult result;
            result.content = *acc;
            result.finishReason = done.finishReason;
            result.usage = *usage;
            result.model = r.model;
            iface->reportResult(result);
            iface->reportFinished();
        });

    QObject::connect(reply.data(), &QNetworkReply::errorOccurred, reply.data(),
        [this, reply, iface, r](QNetworkReply::NetworkError) {
            if (!reply) return;
            AIErrorInfo err;
            err.code = AIError::NetworkError;
            err.message = reply->errorString();
            err.userTag = r.userTag;
            emit requestFailed(err);
            iface->reportException(std::runtime_error(err.message.toStdString()));
            iface->reportFinished();
        });

    return iface->future();
}

// ─── 嵌入 ───────────────────────────────────────────────────────────────────

QFuture<EmbeddingResult> OpenAICompatibleProvider::embed(const EmbeddingRequest& req) {
    auto iface = std::make_shared<QFutureInterface<EmbeddingResult>>();
    iface->reportStarted();

    EmbeddingRequest r = req;
    if (r.model.isEmpty()) r.model = m_config.embeddingModel;

    QNetworkRequest nr((QUrl(embeddingsUrl())));
    nr.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    nr.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.apiKey).toUtf8());

    QPointer<QNetworkReply> reply = m_nam->post(nr, buildEmbeddingBody(r));

    QObject::connect(reply.data(), &QNetworkReply::finished, reply.data(),
        [this, reply, iface, r]() {
            if (!reply) { iface->reportFinished(); return; }
            reply->deleteLater();
            int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray body = reply->readAll();

            if (http >= 400) {
                emit requestFailed(mapError(http, body, r.userTag));
                iface->reportException(std::runtime_error("HTTP " + std::to_string(http)));
                iface->reportFinished();
                return;
            }
            QJsonParseError pe;
            QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
            if (pe.error != QJsonParseError::NoError) {
                AIErrorInfo err;
                err.code = AIError::ResponseFormatInvalid;
                err.userTag = r.userTag;
                emit requestFailed(err);
                iface->reportException(std::runtime_error("Invalid JSON"));
                iface->reportFinished();
                return;
            }
            EmbeddingResult result;
            QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
            for (const QJsonValue& v : data) {
                QJsonArray vec = v.toObject().value(QStringLiteral("embedding")).toArray();
                QVector<float> f;
                f.reserve(vec.size());
                for (const QJsonValue& x : vec) f.append(float(x.toDouble()));
                result.vectors.append(f);
            }
            QJsonObject usage = doc.object().value(QStringLiteral("usage")).toObject();
            result.usage.promptTokens     = usage.value(QStringLiteral("prompt_tokens")).toInt();
            result.usage.totalTokens      = usage.value(QStringLiteral("total_tokens")).toInt();
            result.rawResponse = doc.object();
            iface->reportResult(result);
            iface->reportFinished();
        });

    QObject::connect(reply.data(), &QNetworkReply::errorOccurred, reply.data(),
        [this, reply, iface, r](QNetworkReply::NetworkError) {
            if (!reply) return;
            AIErrorInfo err;
            err.code = AIError::NetworkError;
            err.message = reply->errorString();
            err.userTag = r.userTag;
            emit requestFailed(err);
            iface->reportException(std::runtime_error(err.message.toStdString()));
            iface->reportFinished();
        });

    return iface->future();
}

} // namespace dmc::ai
