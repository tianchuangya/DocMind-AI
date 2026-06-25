// DocMind AI — WritingAssistant 实现
//
// 把 WritingRequest 构造为 system+user 消息，调用 AIProvider。
// 流式版本把 chatStream 的 chunk 转发为 textChunked 信号。
#include "ai/WritingAssistant.h"
#include "core/DocumentSession.h"
#include "knowledge/KnowledgeTypes.h"
#include "utils/Logger.h"

#include <QFutureWatcher>
#include <QPointer>

namespace dmc::ai {

namespace {

// 取 future 结果，异常时返回空（错误已通过 requestFailed 信号通知）
ChatResult safeResult(QFutureWatcher<ChatResult>* w) {
    try {
        return w->result();
    } catch (...) {
        return {};
    }
}

} // namespace

WritingAssistant::WritingAssistant(AIProvider* provider, QObject* parent)
    : QObject(parent), m_provider(provider) {}

std::pair<QString, QString> WritingAssistant::buildPrompts(const WritingRequest& req) const {
    QString systemPrompt = QStringLiteral(
        "你是 DocMind AI 的写作助手。"
        "只输出处理后的文本，不要解释、不要添加额外的元信息。"
        "保留原文的 Markdown 结构（标题、列表、代码块、表格等）。");

    QString userInput = req.selection;
    if (userInput.isEmpty()) userInput = req.wholeDocument;

    if (req.retrievalContext && !req.retrievalContext->reconstructedContext.isEmpty()) {
        systemPrompt += QStringLiteral("\n\n参考知识库内容（仅作为事实依据）：\n")
                      + req.retrievalContext->reconstructedContext;
    }

    QString instruction;
    switch (req.action) {
        case WritingAction::Continue:
            instruction = QStringLiteral("请续写下面这段文本，保持风格与主题一致：\n\n");
            break;
        case WritingAction::Rewrite:
            instruction = QStringLiteral("请改写下面这段文本，意思不变但表达更清晰：\n\n");
            break;
        case WritingAction::Polish:
            instruction = QStringLiteral("请润色下面这段文本，修正语病、提升流畅度，但不要改变含义：\n\n");
            break;
        case WritingAction::Summarize:
            instruction = QStringLiteral("请用简短的中文摘要概括下面这段文本的核心要点（不超过 200 字）：\n\n");
            break;
        case WritingAction::Translate:
            instruction = QStringLiteral("请将下面这段文本翻译成 %1：\n\n")
                              .arg(req.targetLanguage.isEmpty() ? QStringLiteral("英文") : req.targetLanguage);
            break;
        case WritingAction::Custom:
            instruction = req.customInstruction + QStringLiteral("\n\n待处理文本：\n\n");
            break;
    }
    return {systemPrompt, instruction + userInput};
}

QFuture<WritingResult> WritingAssistant::run(const WritingRequest& req) {
    auto iface = std::make_shared<QFutureInterface<WritingResult>>();
    iface->reportStarted();

    if (!m_provider) {
        WritingResult r;
        r.text = QStringLiteral("AI provider 不可用");
        iface->reportResult(r);
        iface->reportFinished();
        return iface->future();
    }

    auto [sys, usr] = buildPrompts(req);
    ChatRequest creq;
    creq.messages.append({Role::System, sys});
    creq.messages.append({Role::User, usr});
    creq.stream = false;
    creq.temperature = 0.7f;
    creq.userTag = req.userTag;

    QFuture<ChatResult> f = m_provider->chat(creq);
    auto w = new QFutureWatcher<ChatResult>();
    QObject::connect(w, &QFutureWatcher<ChatResult>::finished, w,
        [w, iface, req]() {
            w->deleteLater();
            WritingResult r;
            if (!w->isCanceled()) {
                ChatResult cr = safeResult(w);
                r.text = cr.content;
                r.model = cr.model;
            }
            r.usedKnowledgeBase = (req.retrievalContext != nullptr);
            iface->reportResult(r);
            iface->reportFinished();
        });
    w->setFuture(f);
    return iface->future();
}

QFuture<WritingResult> WritingAssistant::runStream(const WritingRequest& req) {
    auto iface = std::make_shared<QFutureInterface<WritingResult>>();
    iface->reportStarted();

    if (!m_provider) {
        WritingResult r;
        r.text = QStringLiteral("AI provider 不可用");
        iface->reportResult(r);
        iface->reportFinished();
        return iface->future();
    }

    QPointer<WritingAssistant> self(this);
    auto [sys, usr] = buildPrompts(req);
    ChatRequest creq;
    creq.messages.append({Role::System, sys});
    creq.messages.append({Role::User, usr});
    creq.stream = true;
    creq.temperature = 0.7f;
    creq.userTag = req.userTag;

    // 转发流式分片到 textChunked
    QObject::connect(m_provider, &AIProvider::chatStreamChunk,
        this, [self, req](const ChatStreamEvent& ev, const QVariant& tag) {
            if (!self) return;
            if (tag != req.userTag) return;
            if (ev.type == ChatStreamEvent::Chunk && !ev.delta.isEmpty()) {
                emit self->textChunked(ev.delta, tag);
            }
        });

    QFuture<ChatResult> f = m_provider->chatStream(creq);
    auto w = new QFutureWatcher<ChatResult>();
    QObject::connect(w, &QFutureWatcher<ChatResult>::finished, w,
        [w, iface, req]() {
            w->deleteLater();
            WritingResult r;
            if (!w->isCanceled()) {
                ChatResult cr = safeResult(w);
                r.text = cr.content;
                r.model = cr.model;
            }
            r.usedKnowledgeBase = (req.retrievalContext != nullptr);
            iface->reportResult(r);
            iface->reportFinished();
        });
    w->setFuture(f);
    return iface->future();
}

WritingRequest WritingAssistant::fromSnapshot(const DocumentSnapshot& snap,
                                                const QString& selectionText,
                                                WritingAction action) {
    WritingRequest r;
    r.action = action;
    r.selection = selectionText;
    r.wholeDocument = snap.content;
    return r;
}

} // namespace dmc::ai
