// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — ThemeManager
// 应用主题管理（浅色/深色主题切换，QSS 样式表管理）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include "core/AppState.h"

namespace dmc {

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager& instance();

    /// 初始化主题系统
    void init();

    /// 获取当前主题
    ThemeMode currentTheme() const { return m_currentTheme; }

    /// 切换主题
    void setTheme(ThemeMode mode);

    /// 切换主题（toggle）
    void toggleTheme();

    /// 获取应用 QSS
    QString applicationQSS() const;

    /// 获取预览 CSS
    QString previewCSS() const;

    /// 获取编辑器前景/背景色
    QColor editorForeground() const;
    QColor editorBackground() const;
    QColor editorSelection() const;

    /// 获取界面颜色
    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor& color);

signals:
    void themeChanged(ThemeMode mode);

private:
    ThemeManager();
    ~ThemeManager() override = default;

    void applyCurrentTheme();
    QString loadLightQSS() const;
    QString loadDarkQSS() const;
    QString loadLightPreviewCSS() const;
    QString loadDarkPreviewCSS() const;

    ThemeMode m_currentTheme = ThemeMode::Light;
    QColor m_accentColor;
    QString m_cachedAppQSS;
    QString m_cachedPreviewCSS;
};

} // namespace dmc
