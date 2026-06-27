// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — ThemeManager 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "widgets/ThemeManager.h"
#include "core/AppState.h"

#include <QApplication>
#include <QFile>
#include <QPalette>

namespace dmc {

ThemeManager& ThemeManager::instance() {
    static ThemeManager s_instance;
    return s_instance;
}

ThemeManager::ThemeManager() {
    m_accentColor = QColor("#4a90d9");
}

void ThemeManager::init() {
    m_currentTheme = AppState::instance().themeMode();
    applyCurrentTheme();
}

void ThemeManager::setTheme(ThemeMode mode) {
    if (m_currentTheme == mode) return;
    m_currentTheme = mode;
    AppState::instance().setThemeMode(mode);
    applyCurrentTheme();
    emit themeChanged(mode);
}

void ThemeManager::toggleTheme() {
    setTheme(m_currentTheme == ThemeMode::Light ? ThemeMode::Dark : ThemeMode::Light);
}

void ThemeManager::applyCurrentTheme() {
    if (m_currentTheme == ThemeMode::Light) {
        m_cachedAppQSS     = loadLightQSS();
        m_cachedPreviewCSS = loadLightPreviewCSS();
    } else {
        m_cachedAppQSS     = loadDarkQSS();
        m_cachedPreviewCSS = loadDarkPreviewCSS();
    }

    QApplication::setStyle("Fusion");
    qApp->setStyleSheet(m_cachedAppQSS);

    // 设置调色板
    QPalette pal;
    if (m_currentTheme == ThemeMode::Light) {
        pal.setColor(QPalette::Window,          QColor("#f5f5f5"));
        pal.setColor(QPalette::WindowText,      QColor("#333333"));
        pal.setColor(QPalette::Base,            QColor("#ffffff"));
        pal.setColor(QPalette::AlternateBase,   QColor("#f0f0f0"));
        pal.setColor(QPalette::ToolTipBase,     QColor("#ffffff"));
        pal.setColor(QPalette::ToolTipText,     QColor("#333333"));
        pal.setColor(QPalette::Text,            QColor("#333333"));
        pal.setColor(QPalette::Button,          QColor("#e8e8e8"));
        pal.setColor(QPalette::ButtonText,      QColor("#333333"));
        pal.setColor(QPalette::BrightText,      QColor("#ff0000"));
        pal.setColor(QPalette::Link,            m_accentColor);
        pal.setColor(QPalette::Highlight,       m_accentColor);
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    } else {
        pal.setColor(QPalette::Window,          QColor("#2b2b2b"));
        pal.setColor(QPalette::WindowText,      QColor("#cccccc"));
        pal.setColor(QPalette::Base,            QColor("#1e1e2e"));
        pal.setColor(QPalette::AlternateBase,   QColor("#252535"));
        pal.setColor(QPalette::ToolTipBase,     QColor("#333333"));
        pal.setColor(QPalette::ToolTipText,     QColor("#cccccc"));
        pal.setColor(QPalette::Text,            QColor("#cdd6f4"));
        pal.setColor(QPalette::Button,          QColor("#353545"));
        pal.setColor(QPalette::ButtonText,      QColor("#cccccc"));
        pal.setColor(QPalette::BrightText,      QColor("#ff6b6b"));
        pal.setColor(QPalette::Link,            QColor("#89b4fa"));
        pal.setColor(QPalette::Highlight,       QColor("#89b4fa"));
        pal.setColor(QPalette::HighlightedText, QColor("#1e1e2e"));
    }
    qApp->setPalette(pal);
}

QString ThemeManager::applicationQSS() const {
    return m_cachedAppQSS;
}

QString ThemeManager::previewCSS() const {
    return m_cachedPreviewCSS;
}

QColor ThemeManager::editorForeground() const {
    return m_currentTheme == ThemeMode::Light
        ? QColor("#1e1e2e") : QColor("#cdd6f4");
}

QColor ThemeManager::editorBackground() const {
    return m_currentTheme == ThemeMode::Light
        ? QColor("#ffffff") : QColor("#1e1e2e");
}

QColor ThemeManager::editorSelection() const {
    return m_currentTheme == ThemeMode::Light
        ? QColor("#b4d7ff") : QColor("#45475a");
}

void ThemeManager::setAccentColor(const QColor& color) {
    m_accentColor = color;
    applyCurrentTheme();
}

// ─── QSS 样式表 ──────────────────────────────────────────────────────────────

QString ThemeManager::loadLightQSS() const {
    return R"(
        QMainWindow {
            background-color: #f5f5f5;
            color: #333333;
        }
        QMenuBar {
            background-color: #f0f0f0;
            color: #333333;
            border-bottom: 1px solid #d0d0d0;
        }
        QMenuBar::item {
            color: #333333;
            padding: 4px 8px;
        }
        QMenuBar::item:selected {
            background-color: #e0e0e0;
            color: #333333;
        }
        QMenu {
            background-color: #ffffff;
            color: #333333;
            border: 1px solid #d0d0d0;
        }
        QMenu::item {
            color: #333333;
            padding: 6px 24px;
        }
        QMenu::item:selected {
            background-color: #4a90d9;
            color: white;
        }
        QMenu::separator {
            height: 1px;
            background-color: #d0d0d0;
            margin: 4px 0px;
        }
        QToolBar {
            background-color: #f0f0f0;
            border-bottom: 1px solid #d0d0d0;
            spacing: 4px;
            padding: 2px;
        }
        QToolButton {
            background-color: transparent;
            color: #333333;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px;
        }
        QToolButton:hover {
            background-color: #e0e0e0;
            border: 1px solid #c0c0c0;
        }
        QToolButton:pressed {
            background-color: #d0d0d0;
        }
        QStatusBar {
            background-color: #e8e8e8;
            color: #555555;
            border-top: 1px solid #d0d0d0;
        }
        QLabel {
            color: #333333;
        }
        QCheckBox {
            color: #333333;
        }
        QComboBox {
            color: #333333;
            background-color: #ffffff;
            border: 1px solid #c0c0c0;
            padding: 2px 6px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #333333;
            selection-background-color: #4a90d9;
            selection-color: white;
        }
        QTabWidget::pane {
            border: 1px solid #d0d0d0;
        }
        QTabBar::tab {
            background-color: #e8e8e8;
            border: 1px solid #d0d0d0;
            padding: 6px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            border-bottom: 2px solid #4a90d9;
        }
        QTabBar::tab:hover:!selected {
            background-color: #e0e0e0;
        }
        QPlainTextEdit {
            background-color: #ffffff;
            color: #1e1e2e;
            border: none;
            selection-background-color: #b4d7ff;
        }
        QTextBrowser {
            background-color: #ffffff;
            border: none;
        }
        QScrollBar:vertical {
            background-color: #f0f0f0;
            width: 12px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background-color: #c0c0c0;
            min-height: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #a0a0a0;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #f0f0f0;
            height: 12px;
        }
        QScrollBar::handle:horizontal {
            background-color: #c0c0c0;
            min-width: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QPushButton {
            background-color: #e8e8e8;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 6px 16px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #d8d8d8;
        }
        QPushButton:pressed {
            background-color: #c8c8c8;
        }
        QLineEdit {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px 8px;
            background-color: #ffffff;
        }
        QLineEdit:focus {
            border-color: #4a90d9;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
        QSplitter::handle {
            background-color: #d0d0d0;
        }
        QSplitter::handle:horizontal {
            width: 2px;
        }
        QSplitter::handle:vertical {
            height: 2px;
        }
        QToolTip {
            background-color: #333;
            color: white;
            border: none;
            padding: 4px;
        }
    )";
}

QString ThemeManager::loadDarkQSS() const {
    return R"(
        QMainWindow {
            background-color: #1e1e2e;
        }
        QMenuBar {
            background-color: #181825;
            border-bottom: 1px solid #45475a;
            color: #cdd6f4;
        }
        QMenuBar::item:selected {
            background-color: #313244;
        }
        QMenu {
            background-color: #1e1e2e;
            border: 1px solid #45475a;
            color: #cdd6f4;
        }
        QMenu::item:selected {
            background-color: #89b4fa;
            color: #1e1e2e;
        }
        QToolBar {
            background-color: #181825;
            border-bottom: 1px solid #45475a;
            spacing: 4px;
            padding: 2px;
        }
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px;
            color: #cdd6f4;
        }
        QToolButton:hover {
            background-color: #313244;
            border: 1px solid #45475a;
        }
        QToolButton:pressed {
            background-color: #45475a;
        }
        QStatusBar {
            background-color: #181825;
            border-top: 1px solid #45475a;
            color: #a6adc8;
        }
        QTabWidget::pane {
            border: 1px solid #45475a;
        }
        QTabBar::tab {
            background-color: #181825;
            border: 1px solid #45475a;
            padding: 6px 16px;
            margin-right: 2px;
            color: #a6adc8;
        }
        QTabBar::tab:selected {
            background-color: #1e1e2e;
            border-bottom: 2px solid #89b4fa;
            color: #cdd6f4;
        }
        QTabBar::tab:hover:!selected {
            background-color: #313244;
        }
        QPlainTextEdit {
            background-color: #1e1e2e;
            color: #cdd6f4;
            border: none;
            selection-background-color: #45475a;
        }
        QTextBrowser {
            background-color: #1e1e2e;
            border: none;
            color: #cdd6f4;
        }
        QScrollBar:vertical {
            background-color: #181825;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background-color: #45475a;
            min-height: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #585b70;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #181825;
            height: 12px;
        }
        QScrollBar::handle:horizontal {
            background-color: #45475a;
            min-width: 20px;
            border-radius: 4px;
            margin: 2px;
        }
        QPushButton {
            background-color: #313244;
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 6px 16px;
            min-width: 80px;
            color: #cdd6f4;
        }
        QPushButton:hover {
            background-color: #45475a;
        }
        QPushButton:pressed {
            background-color: #585b70;
        }
        QLineEdit {
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 4px 8px;
            background-color: #313244;
            color: #cdd6f4;
        }
        QLineEdit:focus {
            border-color: #89b4fa;
        }
        QCheckBox {
            color: #cdd6f4;
        }
        QLabel {
            color: #cdd6f4;
        }
        QSplitter::handle {
            background-color: #45475a;
        }
        QSplitter::handle:horizontal {
            width: 2px;
        }
        QToolTip {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            padding: 4px;
        }
    )";
}

// ─── 预览 CSS ────────────────────────────────────────────────────────────────

QString ThemeManager::loadLightPreviewCSS() const {
    return R"(
        body.markdown-body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            font-size: 15px;
            line-height: 1.7;
            color: #1e1e2e;
            background: #ffffff;
            padding: 20px 30px;
            max-width: 900px;
            margin: 0 auto;
        }
        h1, h2, h3, h4, h5, h6 {
            margin-top: 24px;
            margin-bottom: 16px;
            font-weight: 600;
            line-height: 1.25;
            color: #1a1a2e;
        }
        h1 { font-size: 2em; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }
        h2 { font-size: 1.5em; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }
        h3 { font-size: 1.25em; }
        h4 { font-size: 1em; }
        h5 { font-size: 0.875em; }
        h6 { font-size: 0.85em; color: #6a737d; }
        p { margin: 0 0 16px; }
        a { color: #4a90d9; text-decoration: none; }
        a:hover { text-decoration: underline; }
        strong { font-weight: 600; }
        em { font-style: italic; }
        code {
            font-family: "Consolas", "Monaco", "Courier New", monospace;
            font-size: 0.9em;
            padding: 0.2em 0.4em;
            background-color: #f0f0f0;
            border-radius: 3px;
            color: #c7254e;
        }
        .code-block {
            margin: 16px 0;
            border-radius: 6px;
            overflow: hidden;
            background-color: #f6f8fa;
            border: 1px solid #e1e4e8;
        }
        .code-block .code-lang {
            display: block;
            padding: 6px 12px;
            background-color: #e8eaed;
            font-size: 0.8em;
            color: #586069;
            font-family: sans-serif;
        }
        .code-block pre {
            margin: 0;
            padding: 12px 16px;
            overflow-x: auto;
            font-size: 0.9em;
            line-height: 1.5;
        }
        .code-block code {
            background: none;
            padding: 0;
            color: #24292e;
            font-size: inherit;
        }
        blockquote {
            margin: 16px 0;
            padding: 4px 16px;
            color: #6a737d;
            border-left: 4px solid #dfe2e5;
            background-color: #f8f9fa;
        }
        blockquote p { margin: 8px 0; }
        ul, ol {
            padding-left: 2em;
            margin: 0 0 16px;
        }
        li { margin: 4px 0; }
        li input[type="checkbox"] {
            margin-right: 6px;
            vertical-align: middle;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            margin: 16px 0;
        }
        .table-wrapper {
            overflow-x: auto;
            margin: 16px 0;
        }
        th, td {
            border: 1px solid #dfe2e5;
            padding: 8px 13px;
            text-align: left;
        }
        th {
            background-color: #f6f8fa;
            font-weight: 600;
        }
        tr:nth-child(even) { background-color: #fafbfc; }
        hr {
            height: 2px;
            border: none;
            background-color: #e1e4e8;
            margin: 24px 0;
        }
        img {
            max-width: 100%;
            height: auto;
            border-radius: 4px;
            margin: 8px 0;
        }
        del { color: #999; }
        .footnotes {
            margin-top: 32px;
            padding-top: 16px;
            border-top: 1px solid #eaecef;
            font-size: 0.9em;
            color: #586069;
        }
        .footnote-ref { font-size: 0.8em; vertical-align: super; }
        .footnote-backref { text-decoration: none; color: #4a90d9; }
    )";
}

QString ThemeManager::loadDarkPreviewCSS() const {
    return R"(
        body.markdown-body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            font-size: 15px;
            line-height: 1.7;
            color: #cdd6f4;
            background: #1e1e2e;
            padding: 20px 30px;
            max-width: 900px;
            margin: 0 auto;
        }
        h1, h2, h3, h4, h5, h6 {
            margin-top: 24px;
            margin-bottom: 16px;
            font-weight: 600;
            line-height: 1.25;
            color: #cdd6f4;
        }
        h1 { font-size: 2em; border-bottom: 1px solid #45475a; padding-bottom: 0.3em; }
        h2 { font-size: 1.5em; border-bottom: 1px solid #45475a; padding-bottom: 0.3em; }
        h3 { font-size: 1.25em; }
        h4 { font-size: 1em; }
        h5 { font-size: 0.875em; }
        h6 { font-size: 0.85em; color: #a6adc8; }
        p { margin: 0 0 16px; }
        a { color: #89b4fa; text-decoration: none; }
        a:hover { text-decoration: underline; }
        strong { font-weight: 600; color: #f5c2e7; }
        em { font-style: italic; color: #a6e3a1; }
        code {
            font-family: "Consolas", "Monaco", "Courier New", monospace;
            font-size: 0.9em;
            padding: 0.2em 0.4em;
            background-color: #313244;
            border-radius: 3px;
            color: #f38ba8;
        }
        .code-block {
            margin: 16px 0;
            border-radius: 6px;
            overflow: hidden;
            background-color: #181825;
            border: 1px solid #45475a;
        }
        .code-block .code-lang {
            display: block;
            padding: 6px 12px;
            background-color: #313244;
            font-size: 0.8em;
            color: #a6adc8;
            font-family: sans-serif;
        }
        .code-block pre {
            margin: 0;
            padding: 12px 16px;
            overflow-x: auto;
            font-size: 0.9em;
            line-height: 1.5;
        }
        .code-block code {
            background: none;
            padding: 0;
            color: #cdd6f4;
            font-size: inherit;
        }
        blockquote {
            margin: 16px 0;
            padding: 4px 16px;
            color: #a6adc8;
            border-left: 4px solid #45475a;
            background-color: #181825;
        }
        blockquote p { margin: 8px 0; }
        ul, ol {
            padding-left: 2em;
            margin: 0 0 16px;
        }
        li { margin: 4px 0; }
        li input[type="checkbox"] {
            margin-right: 6px;
            vertical-align: middle;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            margin: 16px 0;
        }
        .table-wrapper {
            overflow-x: auto;
            margin: 16px 0;
        }
        th, td {
            border: 1px solid #45475a;
            padding: 8px 13px;
            text-align: left;
        }
        th {
            background-color: #313244;
            font-weight: 600;
        }
        tr:nth-child(even) { background-color: #181825; }
        hr {
            height: 2px;
            border: none;
            background-color: #45475a;
            margin: 24px 0;
        }
        img {
            max-width: 100%;
            height: auto;
            border-radius: 4px;
            margin: 8px 0;
        }
        del { color: #6c7086; }
        .footnotes {
            margin-top: 32px;
            padding-top: 16px;
            border-top: 1px solid #45475a;
            font-size: 0.9em;
            color: #a6adc8;
        }
        .footnote-ref { font-size: 0.8em; vertical-align: super; }
        .footnote-backref { text-decoration: none; color: #89b4fa; }
    )";
}

} // namespace dmc
