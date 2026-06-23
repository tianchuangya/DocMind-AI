// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — PreviewWidget 实现
// QTextBrowser 只支持 HTML 子集，不能传入完整 HTML 文档
// ─────────────────────────────────────────────────────────────────────────────
#include "preview/PreviewWidget.h"
#include "preview/MarkdownRenderer.h"
#include "preview/ScrollSyncManager.h"
#include "core/AppState.h"

#include <QFile>
#include <QScrollBar>
#include <QWheelEvent>
#include <QTextDocument>
#include <QTextStream>
#include <QApplication>
#include <QStyle>
#include <iostream>

namespace dmc {

PreviewWidget::PreviewWidget(QWidget* parent)
    : QTextBrowser(parent)
{
    m_renderer = new MarkdownRenderer();
    m_scrollSync = new ScrollSyncManager(this);

    // 防抖计时器
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    // 基础配置
    setOpenExternalLinks(false);
    setReadOnly(true);
    setUndoRedoEnabled(false);

    connect(&m_debounceTimer, &QTimer::timeout,
            this, &PreviewWidget::onDebounceTimeout);
    connect(this, &QTextBrowser::anchorClicked,
            this, &PreviewWidget::linkClicked);

    // 初始主题
    applyLightTheme();
}

PreviewWidget::~PreviewWidget() = default;

void PreviewWidget::setMarkdownContent(const QString& markdown, const QString& baseDir) {
    m_currentMarkdown = markdown;
    m_baseDir = baseDir;
    m_pendingRender = true;
    m_debounceTimer.start();
}

void PreviewWidget::renderNow(const QString& markdown, const QString& baseDir) {
    m_currentMarkdown = markdown;
    m_baseDir = baseDir;
    doRender();
}

void PreviewWidget::setThemeCSS(const QString& css) {
    m_themeCSS = css;
    // QTextBrowser 使用 setStyleSheet 或 document()->setDefaultStyleSheet
    document()->setDefaultStyleSheet(css);
    if (!m_currentMarkdown.isEmpty()) {
        doRender();
    }
}

void PreviewWidget::applyLightTheme() {
    QFile file(":/themes/light.css");
    if (file.open(QIODevice::ReadOnly)) {
        m_themeCSS = QString::fromUtf8(file.readAll());
        file.close();
    }

    // 设置 QTextBrowser 的样式表
    document()->setDefaultStyleSheet(m_themeCSS);

    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor("#ffffff"));
    pal.setColor(QPalette::Text, QColor("#1e1e2e"));
    setPalette(pal);

    if (!m_currentMarkdown.isEmpty()) {
        doRender();
    }
}

void PreviewWidget::applyDarkTheme() {
    QFile file(":/themes/dark.css");
    if (file.open(QIODevice::ReadOnly)) {
        m_themeCSS = QString::fromUtf8(file.readAll());
        file.close();
    }

    document()->setDefaultStyleSheet(m_themeCSS);

    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor("#1e1e2e"));
    pal.setColor(QPalette::Text, QColor("#cdd6f4"));
    setPalette(pal);

    if (!m_currentMarkdown.isEmpty()) {
        doRender();
    }
}

void PreviewWidget::onDebounceTimeout() {
    if (m_pendingRender) {
        doRender();
        m_pendingRender = false;
    }
}

void PreviewWidget::doRender() {
    // 配置渲染选项
    RenderOptions opts;
    opts.tables        = true;
    opts.taskLists     = true;
    opts.strikethrough = true;
    opts.autoLink      = true;
    opts.footnotes     = true;
    opts.codeHighlight = true;
    opts.baseUrl       = m_baseDir;

    // 渲染 Markdown → HTML
    QString bodyHtml = m_renderer->render(m_currentMarkdown, opts);
    m_currentHtml = bodyHtml;

    // 记住滚动位置
    int scrollPos = verticalScrollBar()->value();

    // QTextBrowser 只需要 body 内容，样式通过 setDefaultStyleSheet 应用
    setHtml(bodyHtml);

    // 恢复滚动位置
    verticalScrollBar()->setValue(scrollPos);

    // 临时诊断
    std::cerr << "[Preview] 渲染完成: " << m_currentMarkdown.length()
              << " 字符 → " << bodyHtml.length() << " 字符 HTML" << std::endl;

    emit renderFinished();
}

void PreviewWidget::wheelEvent(QWheelEvent* event) {
    // Ctrl + 滚轮 → 缩放预览
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        QFont f = font();
        int size = f.pointSize();

        if (delta > 0 && size < 72) {
            // 放大
            f.setPointSize(size + 1);
            setFont(f);
            // 重新渲染以应用新字体大小
            if (!m_currentMarkdown.isEmpty()) {
                doRender();
            }
        } else if (delta < 0 && size > 8) {
            // 缩小
            f.setPointSize(size - 1);
            setFont(f);
            // 重新渲染以应用新字体大小
            if (!m_currentMarkdown.isEmpty()) {
                doRender();
            }
        }
        event->accept();
        return;
    }
    QTextBrowser::wheelEvent(event);
}

} // namespace dmc
