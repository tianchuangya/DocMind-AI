// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — AIProvider
// AI 服务抽象接口：聊天补全 + 嵌入向量，兼容 OpenAI API。
// 实现见 OpenAICompatibleProvider；首期仅此一种实现。
//
// 设计要点：
//   - 全部异步：返回 QFuture 或通过信号通知，禁止阻塞 UI 线程。
//   - 取消语义：每个请求返回的 QFuture 可 cancel()；取消后不再发信号。
//   - 流式聊天：增量 token 通过 chatStreamChunk 信号实时推送。
//   - 凭据安全：API Key 由调用方从 SecureCredentialStore 取出传入，
//     Provider 不持久化、不写日志原文。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QFuture>
#include <QVariant>
#include <QJsonObject>
#include <optional>

namespace dmc::ai {

// ─── 角色与消息 ───────────────────────────────────────────────────────────────

enum class Role {
    System,
    User,
    Assistant,
    Tool,
};

inline const char* roleToString(Role r) {
    switch (r) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
        case Role::Tool:      return "tool";
    }
    return "user";
}

struct ChatMessage {
    Role    role    = Role::User;
    QString content;
    // 可选：工具调用结果、函数名等，首期可不用
    QString toolCallId;
    QString toolName;
};

// ─── 聊天请求 ─────────────────────────────────────────────────────────────────

struct ChatRequest {
    QList<ChatMessage> messages;
    QString            model;          // 必填，如 "gpt-4o-mini"
    float              temperature  = 0.7f;
    std::optional<int> maxTokens;
    bool               stream        = false;  // true 时走 chatStream，否则走 chat
    float              topP          = 1.0f;
    QStringList        stopSequences;

    // 请求级超时（毫秒）；为空则用 Provider 默认值
    std::optional<int> timeoutMs;

    // 用户标记，用于在信号回调中关联请求（如来源文档 ID）
    QVariant userTag;
};

// ─── 聊天结果 ─────────────────────────────────────────────────────────────────

struct ChatUsage {
    int promptTokens     = 0;
    int completionTokens = 0;
    int totalTokens       = 0;
};

struct ChatResult {
    QString            content;        // 拼好的完整回复
    QString            finishReason;   // "stop" / "length" / "tool_calls"
    ChatUsage          usage;
    QJsonObject        rawResponse;    // 供应商原始 JSON，调试用
    QString            model;          // 实际服务端返回的模型名
};

// ─── 流式分片事件 ──────────────────────────────────────────────────────────────

struct ChatStreamEvent {
    enum Type {
        Chunk,        // 增量文本（delta 非空）
        Usage,        // 用量信息（流末尾，部分供应商提供）
        Done,         // 正常结束
        Error,        // 错误（见 error）
    };
    Type    type   = Type::Chunk;
    QString delta;          // Chunk: 本次新增的文本
    QString finishReason;   // Done: 结束原因
    ChatUsage usage;        // Usage: 用量
};

// ─── 嵌入请求/结果 ─────────────────────────────────────────────────────────────

struct EmbeddingRequest {
    QStringList inputs;            // 单条或多条文本
    QString     model;             // 如 "text-embedding-3-small"
    QString     encodingFormat = QStringLiteral("float");

    std::optional<int> timeoutMs;
    QVariant           userTag;
};

struct EmbeddingResult {
    // 每条输入对应一个向量；顺序与 inputs 一致
    QList<QVector<float>> vectors;
    ChatUsage            usage;     // 供应商返回的 token 用量
    QJsonObject          rawResponse;
};

// ─── 错误 ──────────────────────────────────────────────────────────────────────

enum class AIError {
    None,
    AuthFailed,           // 401/403，API Key 无效
    RateLimited,          // 429
    ServerError,          // 5xx
    NetworkError,         // 连接失败、DNS、断网
    Timeout,              // 超时
    Cancelled,            // 用户取消
    ResponseFormatInvalid,// 响应不是合法 JSON 或缺字段
    ModelUnavailable,     // 模型名错误或不可用
    InvalidRequest,       // 4xx 非认证类
    Unknown,
};

struct AIErrorInfo {
    AIError   code       = AIError::None;
    int       httpStatus = 0;        // 0 表示非 HTTP 错误（如超时/网络）
    QString   providerCode;          // 供应商原始错误码（如 "invalid_api_key"）
    QString   message;               // 安全脱敏后的可读消息（不含 API Key）
    QVariant  userTag;               // 回传请求 userTag
};

// ─── Provider 配置 ─────────────────────────────────────────────────────────────

struct ProviderConfig {
    QString baseUrl;        // 如 "https://api.openai.com"，自动补 /v1
    QString apiKey;         // 由 SecureCredentialStore 注入，不入库
    QString chatModel;      // 默认聊天模型
    QString embeddingModel; // 默认嵌入模型
    int     requestTimeoutMs = 60000;
    QString proxyUrl;       // 可选 HTTP 代理
};

// ─── 抽象接口 ──────────────────────────────────────────────────────────────────

class AIProvider : public QObject {
    Q_OBJECT
public:
    explicit AIProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~AIProvider() override = default;

    // ─── 非流式聊天 ───────────────────────────────────────────────────────────
    // 成功：返回 ChatResult；失败：future 抛出 AIErrorInfo（通过异常或下方的 failed 信号）
    virtual QFuture<ChatResult> chat(const ChatRequest& req) = 0;

    // ─── 流式聊天 ─────────────────────────────────────────────────────────────
    // 立即返回一个句柄对象（自身），调用方 connect chatStreamChunk 信号接收分片。
    // 流结束或出错时分别发 Done / Error 事件。
    // 返回的 future 在流正常结束时提供完整 ChatResult。
    virtual QFuture<ChatResult> chatStream(const ChatRequest& req) = 0;

    // ─── 嵌入 ─────────────────────────────────────────────────────────────────
    virtual QFuture<EmbeddingResult> embed(const EmbeddingRequest& req) = 0;

    // ─── 能力与配置 ───────────────────────────────────────────────────────────
    virtual QString id() const = 0;              // 如 "openai-compatible"
    virtual QString displayName() const = 0;      // UI 显示用
    virtual bool    supportsEmbedding() const = 0;
    virtual void    applyConfig(const ProviderConfig& cfg) = 0;

signals:
    // 流式聊天：每个分片一次
    void chatStreamChunk(const ChatStreamEvent& event, const QVariant& userTag);
    // 任意请求失败（含流式）
    void requestFailed(const AIErrorInfo& error);
};

} // namespace dmc::ai

Q_DECLARE_METATYPE(dmc::ai::ChatResult)
Q_DECLARE_METATYPE(dmc::ai::EmbeddingResult)
Q_DECLARE_METATYPE(dmc::ai::AIErrorInfo)
Q_DECLARE_METATYPE(dmc::ai::ChatStreamEvent)
