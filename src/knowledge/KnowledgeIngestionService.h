// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — KnowledgeIngestionService
// 知识库入库编排：提取（模块 B）→ 分块（ChunkingStrategy）→ 嵌入（AIProvider）→ 存储。
//
// 异步：每一步都在 worker 线程；批量导入时逐文件发信号更新进度。
// 取消：支持 cancelDocument(id) 和 cancelAll()；已入库的部分保留。
//
// 边界：模块 B 的 ConversionService 通过适配器（ConversionServiceAdapter）
//       注入，避免直接依赖 B 的具体类型。
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "knowledge/KnowledgeTypes.h"
#include "knowledge/ChunkingStrategy.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QFuture>
#include <functional>

namespace dmc::ai   { class AIProvider; }
namespace dmc::knowledge { class KnowledgeRepository; }

namespace dmc::knowledge {

// ─── 模块 B 的文本提取适配器（解耦 B 的具体类型） ───────────────────────────
// 实现里把 dmc::knowledge::TextExtractionRequest 转成 B 的类型，结果转回来。
// B 改造期间可注入 mock 适配器便于先开发。
struct ExtractionInput {
    QString sourcePath;
    QString sourceContent;     // 内存源（MD 未保存时）
    QString sourceFormat;      // md/docx/pdf/html
    bool    preferStructure   = true;
};

struct ExtractionOutput {
    QString        markdownText;
    QString        plainText;
    QList<StructBlock> blocks;
    QList<SourceSpan>  spans;
    bool           ok = false;
    int            errorCode = 0;   // 对应 ConversionError 的 int 值
    QString        errorMessage;
};

class ExtractionAdapter {
public:
    virtual ~ExtractionAdapter() = default;
    // 同步实现：IngestionService 会把它放到 worker 线程
    virtual ExtractionOutput extract(const ExtractionInput& in) = 0;
};

// ─── 入库请求 ─────────────────────────────────────────────────────────────────

struct IngestionRequest {
    QString     sourcePath;            // 二选一
    QString     sourceContent;        // 二选一（内存源）
    QString     sourceFormat;
    QString     title;                 // 空则用文件名
    QVariant    userTag;
};

struct IngestionResult {
    qint64    documentId = 0;
    int       chunkCount = 0;
    int       pageCount  = -1;
    bool      ok = false;
    QString   errorMessage;
    QVariant  userTag;
};

// ─── 服务 ─────────────────────────────────────────────────────────────────────

class KnowledgeIngestionService : public QObject {
    Q_OBJECT
public:
    KnowledgeIngestionService(KnowledgeRepository* repo,
                               ai::AIProvider* provider,
                               ExtractionAdapter* extractor,
                               QObject* parent = nullptr);

    void setChunkingOptions(const ChunkingOptions& opts);

    // 单文件入库（异步）
    QFuture<IngestionResult> ingest(const IngestionRequest& req);

    // 批量入库（顺序执行，便于错误隔离；并发度后续可加）
    void ingestBatch(const QList<IngestionRequest>& reqs);

    // 取消
    void cancelDocument(qint64 documentId);
    void cancelAll();

signals:
    // 批量场景：每个文件完成
    void fileIngested(const IngestionResult& result);
    void fileFailed(const QString& sourcePath, const QString& error);
    void batchProgress(int done, int total);
    void batchFinished();

private:
    KnowledgeRepository*  m_repo;
    ai::AIProvider*        m_provider;
    ExtractionAdapter*     m_extractor;
    ChunkingOptions        m_chunkOpts;
};

} // namespace dmc::knowledge
