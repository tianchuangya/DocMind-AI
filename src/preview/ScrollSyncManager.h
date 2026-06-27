// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — ScrollSyncManager
// 编辑器和预览区的滚动同步管理器
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QScrollBar>

namespace dmc {

class MarkdownEditor;
class PreviewWidget;

class ScrollSyncManager : public QObject {
    Q_OBJECT

public:
    explicit ScrollSyncManager(QObject* parent = nullptr);
    ~ScrollSyncManager() override;

    /// 绑定编辑器和预览控件
    void bind(MarkdownEditor* editor, PreviewWidget* preview);

    /// 启用/禁用同步
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /// 独立控制两个方向的同步
    void setEditorToPreviewEnabled(bool enabled);
    bool isEditorToPreviewEnabled() const { return m_editorToPreviewEnabled; }

    void setPreviewToEditorEnabled(bool enabled);
    bool isPreviewToEditorEnabled() const { return m_previewToEditorEnabled; }

    /// 同步模式
    enum class SyncMode {
        Proportional,    // 按比例同步（默认）
        HeadingBased,    // 基于标题同步
        Disabled,
    };

    void setSyncMode(SyncMode mode);
    SyncMode syncMode() const { return m_mode; }

public slots:
    /// 从编辑器同步到预览
    void syncEditorToPreview();

    /// 从预览同步到编辑器
    void syncPreviewToEditor();

private:
    void onEditorScroll();
    void onPreviewScroll();
    void connectSignals();
    void disconnectSignals();

    MarkdownEditor* m_editor  = nullptr;
    PreviewWidget*  m_preview = nullptr;
    QScrollBar*     m_editorScroll  = nullptr;
    QScrollBar*     m_previewScroll = nullptr;

    SyncMode m_mode    = SyncMode::Proportional;
    bool     m_enabled = true;
    bool     m_editorToPreviewEnabled = true;
    bool     m_previewToEditorEnabled = true;
    bool     m_syncing = false;
};

} // namespace dmc
