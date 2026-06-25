// DocMind AI — 模块 C 课程设计 Demo 主窗口
//
// 单窗口布局：
//   ┌─ 设置区（Provider 配置）
//   ├─ 知识库区（导入 / 清空 / 状态）
//   └─ 问答区（提问 → 检索 → LLM 回答 + 来源）
#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QListWidget>

#include "storage/SettingsRepository.h"
#include "storage/SecureCredentialStore.h"
#include "ai/AIProvider.h"
#include "ai/OpenAICompatibleProvider.h"
#include "knowledge/KnowledgeRepository.h"
#include "knowledge/KnowledgeIngestionService.h"
#include "knowledge/KnowledgeQueryService.h"
#include "knowledge/ConversionEngineExtractionAdapter.h"

namespace dmc::conversion { class ConversionEngine; }

namespace dmc::app {

class DemoWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoWindow(QWidget* parent = nullptr);
    ~DemoWindow() override;

private slots:
    void onSaveSettings();
    void onImportFile();
    void onClearKnowledge();
    void onAsk();

    // 信号槽
    void onIngested(const knowledge::IngestionResult& r);
    void onIngestFailed(const QString& path, const QString& err);
    void onBatchProgress(int done, int total, const QString& current);
    void onBatchFinished();

private:
    void setupUi();
    void loadSettings();
    void refreshKnowledgeStatus();
    void log(const QString& msg);
    void refreshProvider();

    // ─── 服务 ─────────────────────────────────────────────────────────────
    storage::SecureCredentialStore*       m_creds  = nullptr;
    storage::SettingsRepository*          m_settings = nullptr;
    ai::OpenAICompatibleProvider*         m_provider = nullptr;
    knowledge::KnowledgeRepository*        m_repo   = nullptr;
    knowledge::KnowledgeIngestionService* m_ingest = nullptr;
    knowledge::KnowledgeQueryService*     m_query  = nullptr;
    knowledge::ConversionEngineExtractionAdapter* m_extractor = nullptr;
    conversion::ConversionEngine*          m_engine = nullptr;

    // ─── UI ────────────────────────────────────────────────────────────────
    QLineEdit* m_baseUrl    = nullptr;
    QLineEdit* m_apiKey     = nullptr;
    QLineEdit* m_chatModel  = nullptr;
    QLineEdit* m_embedModel= nullptr;
    QPushButton* m_saveBtn  = nullptr;

    QPushButton* m_importBtn = nullptr;
    QPushButton* m_clearBtn  = nullptr;
    QLabel*      m_kbStatus  = nullptr;
    QPlainTextEdit* m_log     = nullptr;

    QLineEdit*   m_question  = nullptr;
    QPushButton* m_askBtn     = nullptr;
    QTextEdit*   m_answer     = nullptr;
    QListWidget* m_citations  = nullptr;
};

} // namespace dmc::app
