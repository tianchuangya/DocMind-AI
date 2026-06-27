// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — OpenAICompatibleProvider
// 首期唯一实现：兼容 OpenAI Chat Completions + Embeddings 协议。
//
// 端点：
//   POST {baseUrl}/v1/chat/completions   （支持 stream=true 的 SSE）
//   POST {baseUrl}/v1/embeddings
//
// 认证：Authorization: Bearer <apiKey>
//
// 异步策略：
//   - 基于 QNetworkAccessManager，请求在 Qt 事件循环中完成，不阻塞 UI。
//   - QFuture 通过 QPromise 实现，可在未来切换为 QtConcurrent 实现。
//   - 超时用 QTimer；取消用 future.cancel() 触发 request abort。
//
// 安全：
//   - apiKey 不进日志；错误消息保留供应商消息但屏蔽 key。
//   - 仅在用户主动触发 AI 时调用；知识库问答只发送检索命中的最小上下文。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "ai/AIProvider.h"

#include <QPointer>
#include <QHash>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace dmc::ai {

class OpenAICompatibleProvider : public AIProvider {
    Q_OBJECT
public:
    explicit OpenAICompatibleProvider(QObject* parent = nullptr);
    ~OpenAICompatibleProvider() override;

    QString id() const override          { return QStringLiteral("openai-compatible"); }
    QString displayName() const override { return QStringLiteral("OpenAI Compatible"); }
    bool    supportsEmbedding() const override { return !m_config.embeddingModel.isEmpty(); }
    void    applyConfig(const ProviderConfig& cfg) override;

    QFuture<ChatResult>        chat(const ChatRequest& req) override;
    QFuture<ChatResult>        chatStream(const ChatRequest& req) override;
    QFuture<EmbeddingResult>  embed(const EmbeddingRequest& req) override;

private:
    // 构造请求 URL（自动处理 baseUrl 末尾斜杠与 /v1 前缀）
    QString chatUrl() const;
    QString embeddingsUrl() const;

    // 统一错误映射：HTTP 状态 + 供应商错误体 → AIErrorInfo
    AIErrorInfo mapError(int httpStatus, const QByteArray& body, const QVariant& userTag) const;

    // 解析 SSE 流的一行
    static std::optional<QJsonObject> parseSseLine(const QByteArray& line);

    ProviderConfig m_config;
    QNetworkAccessManager* m_nam = nullptr;
};

} // namespace dmc::ai
