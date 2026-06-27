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
#include <QSplitter>
#include <QSaveFile>
#include <QTextStream>
#include <QTextCursor>
#include <QFutureWatcher>

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

    // 5. 模块 B 转换服务
    m_conversion = new conversion::ConversionService(this);

    // 6. 适配器
    m_extractor = new knowledge::ConversionEngineExtractionAdapter(m_conversion);

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
    updatePreview();
    log(QStringLiteral("DocMind AI 课程设计版启动；DB: ") + dbPath());
}

DemoWindow::~DemoWindow() = default;

void DemoWindow::setupUi() {
    setWindowTitle(QStringLiteral("DocMind AI — 课程设计版"));
    resize(1280, 820);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    auto* leftPane = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPane);
    auto* rightPane = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPane);

    // ── 编辑器区 ───────────────────────────────────────────────────────────
    auto* editorGroup = new QGroupBox(QStringLiteral("① Markdown 编辑器"), leftPane);
    auto* editorLayout = new QVBoxLayout(editorGroup);
    auto* editorBtns = new QHBoxLayout();
    m_newDocBtn = new QPushButton(QStringLiteral("新建"), editorGroup);
    m_openDocBtn = new QPushButton(QStringLiteral("打开"), editorGroup);
    m_saveDocBtn = new QPushButton(QStringLiteral("保存 MD"), editorGroup);
    m_exportHtmlBtn = new QPushButton(QStringLiteral("导出 HTML"), editorGroup);
    m_importEditorBtn = new QPushButton(QStringLiteral("加入知识库"), editorGroup);
    editorBtns->addWidget(m_newDocBtn);
    editorBtns->addWidget(m_openDocBtn);
    editorBtns->addWidget(m_saveDocBtn);
    editorBtns->addWidget(m_exportHtmlBtn);
    editorBtns->addWidget(m_importEditorBtn);
    editorBtns->addStretch();
    editorLayout->addLayout(editorBtns);

    auto* editorSplit = new QSplitter(Qt::Vertical, editorGroup);
    m_editor = new QTextEdit(editorSplit);
    m_editor->setPlaceholderText(QStringLiteral("# 在这里写 Markdown\n\n可以保存、导出 HTML，也可以导入知识库后做问答。"));
    m_preview = new QTextBrowser(editorSplit);
    m_preview->setOpenExternalLinks(true);
    editorSplit->addWidget(m_editor);
    editorSplit->addWidget(m_preview);
    editorSplit->setStretchFactor(0, 3);
    editorSplit->setStretchFactor(1, 2);
    editorLayout->addWidget(editorSplit, 1);

    auto* aiEditBtns = new QHBoxLayout();
    m_polishBtn = new QPushButton(QStringLiteral("AI 润色选中/全文"), editorGroup);
    m_summaryBtn = new QPushButton(QStringLiteral("AI 摘要全文"), editorGroup);
    aiEditBtns->addWidget(m_polishBtn);
    aiEditBtns->addWidget(m_summaryBtn);
    aiEditBtns->addStretch();
    editorLayout->addLayout(aiEditBtns);

    leftLayout->addWidget(editorGroup, 1);

    connect(m_newDocBtn, &QPushButton::clicked, this, &DemoWindow::onNewDocument);
    connect(m_openDocBtn, &QPushButton::clicked, this, &DemoWindow::onOpenDocument);
    connect(m_saveDocBtn, &QPushButton::clicked, this, &DemoWindow::onSaveDocument);
    connect(m_exportHtmlBtn, &QPushButton::clicked, this, &DemoWindow::onExportHtml);
    connect(m_importEditorBtn, &QPushButton::clicked, this, &DemoWindow::onImportEditorToKnowledge);
    connect(m_polishBtn, &QPushButton::clicked, this, &DemoWindow::onPolishSelection);
    connect(m_summaryBtn, &QPushButton::clicked, this, &DemoWindow::onSummarizeDocument);
    connect(m_editor, &QTextEdit::textChanged, this, &DemoWindow::updatePreview);

    // ── 设置区 ────────────────────────────────────────────────────────────
    auto* settingGroup = new QGroupBox(QStringLiteral("② AI 服务设置"), rightPane);
    auto* form = new QFormLayout(settingGroup);
    m_baseUrl   = new QLineEdit(QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode"), settingGroup);
    m_embeddingBaseUrl = new QLineEdit(QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode"), settingGroup);
    m_apiKey    = new QLineEdit(settingGroup);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_chatModel = new QLineEdit(QStringLiteral("qwen-plus"), settingGroup);
    m_embedModel= new QLineEdit(QStringLiteral("text-embedding-v4"), settingGroup);
    m_saveBtn   = new QPushButton(QStringLiteral("保存设置"), settingGroup);
    form->addRow(QStringLiteral("文本 Base URL:"), m_baseUrl);
    form->addRow(QStringLiteral("向量 Base URL:"), m_embeddingBaseUrl);
    form->addRow(QStringLiteral("API Key:"),     m_apiKey);
    form->addRow(QStringLiteral("聊天模型:"),    m_chatModel);
    form->addRow(QStringLiteral("嵌入模型:"),    m_embedModel);
    form->addRow(QString(),                       m_saveBtn);
    connect(m_saveBtn, &QPushButton::clicked, this, &DemoWindow::onSaveSettings);

    // ── 知识库区 ───────────────────────────────────────────────────────────
    auto* kbGroup = new QGroupBox(QStringLiteral("③ 知识库"), rightPane);
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
    auto* askGroup = new QGroupBox(QStringLiteral("④ AI 问答"), rightPane);
    auto* askLayout = new QVBoxLayout(askGroup);
    auto* askRow = new QHBoxLayout();
    m_question = new QLineEdit(askGroup);
    m_question->setPlaceholderText(QStringLiteral("问知识库；或输入 general: 直接问 AI"));
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
    m_log = new QPlainTextEdit(rightPane);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    QFont monoFont(QStringLiteral("Menlo"));
    monoFont.setStyleHint(QFont::Monospace);
    m_log->setFont(monoFont);

    rightLayout->addWidget(settingGroup);
    rightLayout->addWidget(kbGroup);
    rightLayout->addWidget(askGroup, 1);
    rightLayout->addWidget(new QLabel(QStringLiteral("日志:"), rightPane));
    rightLayout->addWidget(m_log, 1);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    setCentralWidget(central);
}

QString DemoWindow::editorTitle() const {
    if (!m_currentFilePath.isEmpty()) return QFileInfo(m_currentFilePath).fileName();
    return QStringLiteral("未命名文档.md");
}

void DemoWindow::updatePreview() {
    if (!m_preview || !m_editor) return;
    m_preview->setMarkdown(m_editor->toPlainText());
}

void DemoWindow::onNewDocument() {
    m_currentFilePath.clear();
    m_editor->clear();
    m_editor->setFocus();
    log(QStringLiteral("✓ 新建文档"));
}

void DemoWindow::onOpenDocument() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 Markdown / 文本文档"),
        QString(),
        QStringLiteral("文档 (*.md *.markdown *.txt *.html *.htm);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), f.errorString());
        return;
    }
    m_currentFilePath = path;
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    log(QStringLiteral("✓ 已打开: ") + path);
}

void DemoWindow::onSaveDocument() {
    QString path = m_currentFilePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存 Markdown"),
            QDir::homePath() + QStringLiteral("/untitled.md"),
            QStringLiteral("Markdown (*.md);;Text (*.txt);;所有文件 (*)"));
        if (path.isEmpty()) return;
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), f.errorString());
        return;
    }
    f.write(m_editor->toPlainText().toUtf8());
    if (!f.commit()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), f.errorString());
        return;
    }
    m_currentFilePath = path;
    log(QStringLiteral("✓ 已保存: ") + path);
}

void DemoWindow::onExportHtml() {
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 HTML"),
        QDir::homePath() + QStringLiteral("/docmind-export.html"),
        QStringLiteral("HTML (*.html)"));
    if (path.isEmpty()) return;

    const QString html = QStringLiteral(
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<title>%1</title><style>body{max-width:860px;margin:40px auto;"
        "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "line-height:1.7;padding:0 20px;}pre,code{background:#f6f8fa;}"
        "pre{padding:12px;overflow:auto;}blockquote{color:#666;border-left:4px solid #ddd;padding-left:12px;}</style>"
        "</head><body>%2</body></html>")
        .arg(editorTitle().toHtmlEscaped(), m_editor->document()->toHtml());

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), f.errorString());
        return;
    }
    f.write(html.toUtf8());
    if (!f.commit()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), f.errorString());
        return;
    }
    log(QStringLiteral("✓ 已导出 HTML: ") + path);
}

void DemoWindow::onImportEditorToKnowledge() {
    const QString content = m_editor->toPlainText().trimmed();
    if (content.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("编辑器内容为空。"));
        return;
    }
    knowledge::IngestionRequest r;
    r.sourceContent = content;
    r.sourceFormat = QStringLiteral("md");
    r.title = editorTitle();
    log(QStringLiteral("→ 正在把当前编辑器内容加入知识库"));
    m_ingest->ingest(r);
}

void DemoWindow::runEditorAiAction(const QString& instruction,
                                   bool replaceSelection,
                                   const QString& label) {
    refreshProvider();
    QString input = m_editor->textCursor().selectedText();
    input.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    if (input.trimmed().isEmpty()) input = m_editor->toPlainText();
    if (input.trimmed().isEmpty()) return;

    ai::ChatRequest req;
    req.model = m_chatModel->text().trimmed();
    req.temperature = 0.4f;
    req.messages.append({ai::Role::System,
        QStringLiteral("你是 DocMind AI 写作助手。保留 Markdown 结构，只输出处理后的正文。")});
    req.messages.append({ai::Role::User, instruction + QStringLiteral("\n\n") + input});

    m_polishBtn->setEnabled(false);
    m_summaryBtn->setEnabled(false);
    log(QStringLiteral("→ ") + label);

    QFuture<ai::ChatResult> f = m_provider->chat(req);
    auto* w = new QFutureWatcher<ai::ChatResult>(this);
    connect(w, &QFutureWatcher<ai::ChatResult>::finished, this,
        [this, w, replaceSelection, label]() {
            w->deleteLater();
            m_polishBtn->setEnabled(true);
            m_summaryBtn->setEnabled(true);
            try {
                const ai::ChatResult r = w->result();
                if (replaceSelection && m_editor->textCursor().hasSelection()) {
                    QTextCursor c = m_editor->textCursor();
                    c.insertText(r.content);
                } else {
                    m_answer->setPlainText(r.content);
                }
                log(QStringLiteral("✓ ") + label + QStringLiteral("完成"));
            } catch (const std::exception& e) {
                m_answer->setPlainText(QStringLiteral("AI 调用失败: ") + QString::fromUtf8(e.what()));
                log(QStringLiteral("✗ ") + label + QStringLiteral("失败"));
            }
        });
    w->setFuture(f);
}

void DemoWindow::onPolishSelection() {
    runEditorAiAction(QStringLiteral("请润色下面的 Markdown 文本，使表达更自然清晰，不改变原意。"),
                      true,
                      QStringLiteral("AI 润色"));
}

void DemoWindow::onSummarizeDocument() {
    runEditorAiAction(QStringLiteral("请总结下面文档的核心内容，输出 5 条以内要点。"),
                      false,
                      QStringLiteral("AI 摘要"));
}

void DemoWindow::loadSettings() {
    if (!m_settings) return;
    auto p = m_settings->defaultProvider();
    if (!p) {
        // 首次启动填默认值；保存时才落库
        return;
    }
    m_baseUrl->setText(p->baseUrl);
    m_embeddingBaseUrl->setText(p->embeddingBaseUrl.isEmpty() ? p->baseUrl : p->embeddingBaseUrl);
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
    cfg.embeddingBaseUrl= m_embeddingBaseUrl->text().trimmed();
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
    s.embeddingBaseUrl= m_embeddingBaseUrl->text().trimmed();
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
    refreshProvider();
    m_askBtn->setEnabled(false);
    m_answer->clear();
    m_citations->clear();

    if (q.startsWith(QStringLiteral("general:"), Qt::CaseInsensitive)) {
        const QString prompt = q.mid(QStringLiteral("general:").size()).trimmed();
        ai::ChatRequest req;
        req.model = m_chatModel->text().trimmed();
        req.messages.append({ai::Role::System, QStringLiteral("你是 DocMind AI 助手，回答要简洁清楚。")});
        req.messages.append({ai::Role::User, prompt});
        m_answer->setPlainText(QStringLiteral("AI 思考中..."));
        QFuture<ai::ChatResult> f = m_provider->chat(req);
        auto* w = new QFutureWatcher<ai::ChatResult>(this);
        connect(w, &QFutureWatcher<ai::ChatResult>::finished, this, [this, w]() {
            w->deleteLater();
            try {
                m_answer->setPlainText(w->result().content);
            } catch (const std::exception& e) {
                m_answer->setPlainText(QStringLiteral("AI 调用失败: ") + QString::fromUtf8(e.what()));
            }
            m_askBtn->setEnabled(true);
        });
        w->setFuture(f);
        return;
    }

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

void DemoWindow::onBatchFinished() {
    log(QStringLiteral("✓ 批量导入完成"));
    refreshKnowledgeStatus();
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
