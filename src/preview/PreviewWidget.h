// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — PreviewWidget
// Markdown 预览控件（基于 QTextBrowser，支持 HTML + CSS 渲染）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QTextBrowser>
#include <QTimer>
#include <QString>

namespace dmc {

class MarkdownRenderer;
class ScrollSyncManager;

class PreviewWidget : public QTextBrowser {
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget() override;

    /// 设置 Markdown 内容（触发防抖渲染）
    void setMarkdownContent(const QString& markdown, const QString& baseDir = "");

    /// 立即渲染（跳过防抖）
    void renderNow(const QString& markdown, const QString& baseDir = "");

    /// 获取渲染器
    MarkdownRenderer* renderer() const { return m_renderer; }

    /// 获取滚动同步管理器
    ScrollSyncManager* scrollSync() const { return m_scrollSync; }

    /// 设置 CSS 主题
    void setThemeCSS(const QString& css);

    /// 加载浅色主题
    void applyLightTheme();

    /// 加载深色主题
    void applyDarkTheme();

    /// 获取当前 HTML
    QString currentHtml() const { return m_currentHtml; }

    /// 获取当前 Markdown 源
    QString currentMarkdown() const { return m_currentMarkdown; }

signals:
    void renderFinished();
    void linkClicked(const QUrl& url);

protected:
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onDebounceTimeout();

private:
    void doRender();
    QString wrapHtml(const QString& bodyHtml) const;

    MarkdownRenderer*   m_renderer   = nullptr;
    ScrollSyncManager*  m_scrollSync = nullptr;

    QString m_currentHtml;
    QString m_currentMarkdown;
    QString m_baseDir;
    QString m_themeCSS;

    QTimer  m_debounceTimer;
    bool    m_pendingRender = false;
};

} // namespace dmc
