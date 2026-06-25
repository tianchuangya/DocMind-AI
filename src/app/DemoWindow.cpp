// DocMind AI — DemoWindow 实现
#include "app/DemoWindow.h"
#include "utils/Logger.h"
#include "conversion/ConversionService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QListWidgetItem>
#include <QFont>
#include <QApplication>

namespace dmc::app {

namespace {
QString dbPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.docmindai");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/docmind.db");
}
}

DemoWindow::DemoWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUi();

    QString err;
    // 初始化日志
    Logger::instance().init(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    // 1. 凭据存储
    m_creds = new storage::SecureCredentialStore(this);

    // 2. 设置仓库
    m_settings = new storage::SettingsRepository(dbPath(), m_creds, this);
    if (!m_settings->open(&err)) {
        log(QStringLiteral("Settings DB 打开失败: ") + err);
    }

    // 3. AI Provider
    m_provider = new ai::OpenAICompatibleProvider(this);

    // 4. 知识库仓储
    m_repo = new knowledge::KnowledgeRepository(dbPath(), this);
    if (!m_repo->open(&err)) {
        log(QStringLiteral("Knowledge DB 打开失败: ") + err);
    }

    // 5. 模块 B 转换引擎
    m_engine = new conversion::ConversionEngine(this);

    // 6. 适配器
    m_extractor = new knowledge::ConversionEngineExtractionAdapter(m_engine);

    // 7. 入库与查询服务
    m_ingest = new knowledge::KnowledgeIngestionService(m_repo, m_provider, m_extractor, this);
    m_query  = new knowledge::KnowledgeQueryService(m_repo, m_provider, this);

    // 信号连接
    connect(m_ingest, &knowledge::KnowledgeIngestionService::fileIngested,
            this, &DemoWindow::onIngested);
    connect(m_ingest, &knowledge::KnowledgeIngestionService::fileFailed,
            this, &DemoWindow::onIngestFailed);
    connect(m_ingest, &knowledge::KnowledgeIngestionService::batchProgress,
            this, &DemoWindow::onBatchProgress);
    connect(m_ingest, &knowledge::KnowledgeIngestionService::batchFinished,
            this, [this]() {
        log(QStringLiteral("✓ 批量导入完成"));
        refreshKnowledgeStatus();
    });
    connect(m_provider, &ai::AIProvider::requestFailed,
            this, [this](const ai::AIErrorInfo& e) {
        log(QStringLiteral("✗ AI 请求失败 [") + QString::number(int(e.code)) +
            QStringLiteral("] ") + e.message);
    });

    loadSettings();
    refreshKnowledgeStatus();
    log(QStringLiteral("DocMind AI Demo 启动；DB: ") + dbPath());
}

DemoWindow::~DemoWindow() = default;

void DemoWindow::setupUi() {
    setWindowTitle(QStringLiteral("DocMind AI — 模块 C Demo"));
    resize(900, 700);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // ── 设置区 ────────────────────────────────────────────────────────────
    auto* settingGroup = new QGroupBox(QStringLiteral("① AI 服务设置"), central);
    auto* form = new QFormLayout(settingGroup);
    m_baseUrl   = new QLineEdit(QStringLiteral("https://api.openai.com"), settingGroup);
    m_apiKey    = new QLineEdit(settingGroup);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_chatModel = new QLineEdit(QStringLiteral("gpt-4o-mini"), settingGroup);
    m_embedModel= new QLineEdit(QStringLiteral("text-embedding-3-small"), settingGroup);
    m_saveBtn   = new QPushButton(QStringLiteral("保存设置"), settingGroup);
    form->addRow(QStringLiteral("Base URL:"),    m_baseUrl);
    form->addRow(QStringLiteral("API Key:"),     m_apiKey);
    form->addRow(QStringLiteral("聊天模型:"),    m_chatModel);
    form->addRow(QStringLiteral("嵌入模型:"),    m_embedModel);
    form->addRow(QString(),                       m_saveBtn);
    connect(m_saveBtn, &QPushButton::clicked, this, &DemoWindow::onSaveSettings);

    // ── 知识库区 ───────────────────────────────────────────────────────────
    auto* kbGroup = new QGroupBox(QStringLiteral("② 知识库"), central);
    auto* kbLayout = new QVBoxLayout(kbGroup);
    auto* kbBtnRow = new QHBoxLayout();
    m_importBtn = new QPushButton(QStringLiteral("导入文件 (MD/DOCX/PDF/HTML)"), kbGroup);
    m_clearBtn  = new QPushButton(QStringLiteral("清空知识库"), kbGroup);
    kbBtnRow->addWidget(m_importBtn);
    kbBtnRow->addWidget(m_clearBtn);
    kbBtnRow->addStretch();
    kbLayout->addLayout(kbBtnRow);
    m_kbStatus = new QLabel(QStringLiteral("尚未导入任何文件"), kbGroup);
    kbLayout->addWidget(m_kbStatus);
    connect(m_importBtn, &QPushButton::clicked, this, &DemoWindow::onImportFile);
    connect(m_clearBtn,  &QPushButton::clicked, this, &DemoWindow::onClearKnowledge);

    // ── 问答区 ─────────────────────────────────────────────────────────────
    auto* askGroup = new QGroupBox(QStringLiteral("③ 知识库问答"), central);
    auto* askLayout = new QVBoxLayout(askGroup);
    auto* askRow = new QHBoxLayout();
    m_question = new QLineEdit(askGroup);
    m_question->setPlaceholderText(QStringLiteral("问点啥..."));
    m_askBtn   = new QPushButton(QStringLiteral("提问"), askGroup);
    askRow->addWidget(m_question);
    askRow->addWidget(m_askBtn);
    askLayout->addLayout(askRow);

    m_answer = new QTextEdit(askGroup);
    m_answer->setReadOnly(true);
    m_answer->setPlaceholderText(QStringLiteral("LLM 回答会显示在这里"));
    askLayout->addWidget(new QLabel(QStringLiteral("回答:"), askGroup));
    askLayout->addWidget(m_answer);

    m_citations = new QListWidget(askGroup);
    m_citations->setMaximumHeight(120);
    askLayout->addWidget(new QLabel(QStringLiteral("来源引用:"), askGroup));
    askLayout->addWidget(m_citations);
    connect(m_askBtn, &QPushButton::clicked, this, &DemoWindow::onAsk);
    connect(m_question, &QLineEdit::returnPressed, this, &DemoWindow::onAsk);

    // ── 日志 ──────────────────────────────────────────────────────────────
    m_log = new QPlainTextEdit(central);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    QFont monoFont(QStringLiteral("Menlo"));
    monoFont.setStyleHint(QFont::Monospace);
    m_log->setFont(monoFont);

    root->addWidget(settingGroup);
    root->addWidget(kbGroup);
    root->addWidget(askGroup, 1);
    root->addWidget(new QLabel(QStringLiteral("日志:"), central));
    root->addWidget(m_log, 1);

    setCentralWidget(central);
}

void DemoWindow::loadSettings() {
    if (!m_settings) return;
    auto p = m_settings->defaultProvider();
    if (!p) {
        // 首次启动填默认值；保存时才落库
        return;
    }
    m_baseUrl->setText(p->baseUrl);
    m_chatModel->setText(p->chatModel);
    m_embedModel->setText(p->embeddingModel);
    // API Key 从凭据取回显示到输入框（仅占位，安全考虑可省略）
    if (m_creds && !p->apiKeyRef.isEmpty()) {
        auto key = m_creds->load(p->apiKeyRef);
        if (key) m_apiKey->setText(*key);
    }
    refreshProvider();
}

void DemoWindow::refreshProvider() {
    ai::ProviderConfig cfg;
    cfg.baseUrl         = m_baseUrl->text().trimmed();
    cfg.apiKey          = m_apiKey->text().trimmed();
    cfg.chatModel       = m_chatModel->text().trimmed();
    cfg.embeddingModel  = m_embedModel->text().trimmed();
    m_provider->applyConfig(cfg);
}

void DemoWindow::onSaveSettings() {
    if (!m_settings) return;
    storage::ProviderSettings s;
    s.displayName     = QStringLiteral("OpenAI Compatible");
    s.baseUrl         = m_baseUrl->text().trimmed();
    s.chatModel       = m_chatModel->text().trimmed();
    s.embeddingModel  = m_embedModel->text().trimmed();
    s.isDefault       = true;

    // API Key 写入钥匙串
    if (!m_apiKey->text().trimmed().isEmpty()) {
        s.apiKeyRef = QStringLiteral("openai-default");
        m_creds->store(s.apiKeyRef, m_apiKey->text().trimmed());
    }

    qint64 id = m_settings->upsertProvider(s);
    if (id > 0) {
        log(QStringLiteral("✓ 配置已保存 (provider id=") + QString::number(id) + QStringLiteral(")"));
    } else {
        log(QStringLiteral("✗ 保存失败"));
    }
    refreshProvider();
}

void DemoWindow::onImportFile() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择文件导入"),
        QString(),
        QStringLiteral("文档 (*.md *.markdown *.docx *.pdf *.html *.htm);;所有文件 (*)"));
    if (files.isEmpty()) return;

    QList<knowledge::IngestionRequest> reqs;
    for (const QString& f : files) {
        knowledge::IngestionRequest r;
        r.sourcePath   = f;
        r.sourceFormat = QFileInfo(f).suffix();
        r.title        = QFileInfo(f).fileName();
        reqs.append(r);
    }
    log(QStringLiteral("→ 开始导入 ") + QString::number(reqs.size()) + QStringLiteral(" 个文件"));
    m_ingest->ingestBatch(reqs);
}

void DemoWindow::onClearKnowledge() {
    auto ret = QMessageBox::question(
        this, QStringLiteral("确认"),
        QStringLiteral("将清空所有文档、分块、向量。继续？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    if (m_repo && m_repo->clearAll()) {
        log(QStringLiteral("✓ 知识库已清空"));
    }
    refreshKnowledgeStatus();
}

void DemoWindow::onAsk() {
    QString q = m_question->text().trimmed();
    if (q.isEmpty()) return;
    m_askBtn->setEnabled(false);
    m_answer->clear();
    m_citations->clear();
    m_answer->setPlainText(QStringLiteral("检索中..."));

    // 1. 知识库检索
    QFuture<knowledge::RetrievalResult> rf = m_query->retrieve(q);
    auto* w = new QFutureWatcher<knowledge::RetrievalResult>(this);
    connect(w, &QFutureWatcher<knowledge::RetrievalResult>::finished, this,
        [this, w, q]() {
            w->deleteLater();
            knowledge::RetrievalResult rr = w->result();

            // 展示来源
            m_citations->clear();
            for (const knowledge::Citation& c : rr.citations) {
                QString s = c.documentTitle;
                if (c.page >= 0) s += QStringLiteral(" · 第%1页").arg(c.page);
                else if (!c.anchor.isEmpty()) s += QStringLiteral(" · #%1").arg(c.anchor);
                s += QStringLiteral(" · score=%1").arg(c.score, 0, 'f', 3);
                auto* item = new QListWidgetItem(s, m_citations);
                item->setToolTip(c.snippet);
                m_citations->addItem(item);
            }
            if (rr.citations.isEmpty()) {
                m_answer->setPlainText(QStringLiteral("⚠ 知识库无命中；如需直接对话请使用 'general: ' 前缀。"));
                m_askBtn->setEnabled(true);
                return;
            }

            // 2. 调用 LLM，注入检索上下文
            ai::ChatRequest creq;
            creq.messages.append({ai::Role::System,
                QStringLiteral("你是一个知识库问答助手。仅根据下面的参考资料回答用户问题。"
                                "若资料中没有答案，明确说明 '资料中未提及'。"
                                "回答末尾列出引用编号。")});
            creq.messages.append({ai::Role::System, rr.reconstructedContext});
            creq.messages.append({ai::Role::User, q});
            creq.model = m_chatModel->text().trimmed();

            QFuture<ai::ChatResult> cf = m_provider->chat(creq);
            auto* cw = new QFutureWatcher<ai::ChatResult>(this);
            connect(cw, &QFutureWatcher<ai::ChatResult>::finished, this, [this, cw]() {
                cw->deleteLater();
                try {
                    ai::ChatResult r = cw->result();
                    m_answer->setPlainText(r.content.isEmpty()
                        ? QStringLiteral("(空回复)") : r.content);
                } catch (const std::exception& e) {
                    m_answer->setPlainText(QStringLiteral("✗ 失败: ") + QString::fromUtf8(e.what()));
                }
                m_askBtn->setEnabled(true);
            });
            cw->setFuture(cf);
        });
    w->setFuture(rf);
}

void DemoWindow::onIngested(const knowledge::IngestionResult& r) {
    log(QStringLiteral("✓ ") + QString::number(r.documentId) +
        QStringLiteral(" 入库成功, ") + QString::number(r.chunkCount) +
        QStringLiteral(" 个分块") + (r.pageCount > 0 ? QStringLiteral(", %1 页").arg(r.pageCount) : QString()));
    refreshKnowledgeStatus();
}

void DemoWindow::onIngestFailed(const QString& path, const QString& err) {
    log(QStringLiteral("✗ 导入失败 [") + QFileInfo(path).fileName() + QStringLiteral("]: ") + err);
}

void DemoWindow::onBatchProgress(int done, int total, const QString& current) {
    m_kbStatus->setText(QStringLiteral("进度: %1/%2 (%3)")
                          .arg(done).arg(total).arg(QFileInfo(current).fileName()));
}

void DemoWindow::refreshKnowledgeStatus() {
    if (!m_repo) return;
    auto docs = m_repo->allDocuments();
    int total = 0, chunks = 0;
    for (const auto& d : docs) {
        total++;
        chunks += d.chunkCount;
    }
    m_kbStatus->setText(QStringLiteral("已导入 %1 个文档，共 %2 个分块").arg(total).arg(chunks));
}

void DemoWindow::log(const QString& msg) {
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                            .arg(msg));
}

} // namespace dmc::app
