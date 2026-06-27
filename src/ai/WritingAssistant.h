// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — WritingAssistant
// 写作助手：基于当前选区或全文，提供续写/改写/润色/摘要/翻译/自定义指令。
//
// 关键约束（来自 PLAN）：
//   - 所有结果先在 AI 面板展示，由用户显式选择插入/替换/复制。
//     本服务**不直接修改 DocumentSession**，只产出结果文本交给 UI。
//   - 知识库可选：若启用 RAG，检索命中作为 system/user 上下文注入。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "ai/AIProvider.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QFuture>
#include <optional>

namespace dmc {

struct DocumentSnapshot;

namespace knowledge { struct RetrievalResult; }

namespace ai {

enum class WritingAction {
    Continue,       // 续写
    Rewrite,        // 改写
    Polish,         // 润色
    Summarize,      // 摘要
    Translate,      // 翻译（targetLanguage 生效）
    Custom,          // 自定义指令（customInstruction 生效）
};

struct WritingRequest {
    WritingAction action = WritingAction::Polish;

    // 输入文本：优先 selection；为空时使用 wholeDocument
    QString selection;
    QString wholeDocument;

    // 翻译目标语言（仅 Translate 使用，如 "English"、"简体中文"）
    QString targetLanguage;

    // 自定义指令（仅 Custom 使用）
    QString customInstruction;

    // 可选：知识库检索结果，作为额外上下文
    const knowledge::RetrievalResult* retrievalContext = nullptr;

    // 用户标记
    QVariant userTag;
};

struct WritingResult {
    QString text;             // 助手产出文本
    QString model;            // 实际使用的模型
    // 是否基于知识库（用于 UI 显示来源折叠区）
    bool    usedKnowledgeBase = false;
};

class WritingAssistant : public QObject {
    Q_OBJECT
public:
    explicit WritingAssistant(AIProvider* provider, QObject* parent = nullptr);

    // 执行一次写作操作（非流式）
    QFuture<WritingResult> run(const WritingRequest& req);

    // 执行一次写作操作（流式）；增量通过 textChunked 信号推送
    // 最终结果通过 future 返回，结束发 finished 信号
    QFuture<WritingResult> runStream(const WritingRequest& req);

    // 把 DocumentSnapshot + 选区组合成 WritingRequest 的便捷构造
    // selectionText 由 UI 从 MarkdownEditor::selectedText() 取
    static WritingRequest fromSnapshot(const DocumentSnapshot& snap,
                                        const QString& selectionText,
                                        WritingAction action);

signals:
    // 流式增量
    void textChunked(const QString& delta, const QVariant& userTag);
    // 任意请求失败
    void failed(const AIErrorInfo& error);

private:
    // 根据 action + 请求构造 system prompt 和 user prompt
    std::pair<QString, QString> buildPrompts(const WritingRequest& req) const;

    AIProvider* m_provider;
};

} // namespace dmc::ai
} // namespace dmc
