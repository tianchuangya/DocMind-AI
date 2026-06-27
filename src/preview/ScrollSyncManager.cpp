// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — ScrollSyncManager 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "preview/ScrollSyncManager.h"
#include "editor/MarkdownEditor.h"
#include "preview/PreviewWidget.h"

#include <QScrollBar>
#include <QTextBlock>
#include <cmath>

namespace dmc {

ScrollSyncManager::ScrollSyncManager(QObject* parent)
    : QObject(parent)
{
}

ScrollSyncManager::~ScrollSyncManager() = default;

void ScrollSyncManager::bind(MarkdownEditor* editor, PreviewWidget* preview) {
    disconnectSignals();

    m_editor  = editor;
    m_preview = preview;

    connectSignals();
}

void ScrollSyncManager::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled) {
        disconnectSignals();
    } else {
        connectSignals();
    }
}

void ScrollSyncManager::setEditorToPreviewEnabled(bool enabled) {
    m_editorToPreviewEnabled = enabled;
}

void ScrollSyncManager::setPreviewToEditorEnabled(bool enabled) {
    m_previewToEditorEnabled = enabled;
}

void ScrollSyncManager::setSyncMode(SyncMode mode) {
    m_mode = mode;
}

void ScrollSyncManager::connectSignals() {
    if (!m_editor || !m_preview) return;

    m_editorScroll = m_editor->verticalScrollBar();
    m_previewScroll = m_preview->verticalScrollBar();

    if (m_editorScroll) {
        connect(m_editorScroll, &QScrollBar::valueChanged,
                this, &ScrollSyncManager::onEditorScroll);
    }
    if (m_previewScroll) {
        connect(m_previewScroll, &QScrollBar::valueChanged,
                this, &ScrollSyncManager::onPreviewScroll);
    }
}

void ScrollSyncManager::disconnectSignals() {
    if (m_editorScroll) {
        disconnect(m_editorScroll, &QScrollBar::valueChanged,
                   this, &ScrollSyncManager::onEditorScroll);
    }
    if (m_previewScroll) {
        disconnect(m_previewScroll, &QScrollBar::valueChanged,
                   this, &ScrollSyncManager::onPreviewScroll);
    }
    m_editorScroll  = nullptr;
    m_previewScroll = nullptr;
}

void ScrollSyncManager::onEditorScroll() {
    if (!m_enabled || !m_editorToPreviewEnabled || m_syncing || !m_preview || !m_editorScroll || !m_previewScroll) {
        return;
    }

    m_syncing = true;

    if (m_mode == SyncMode::Proportional) {
        // 按比例同步
        int editorMax = m_editorScroll->maximum();
        int previewMax = m_previewScroll->maximum();

        if (editorMax > 0 && previewMax > 0) {
            double ratio = static_cast<double>(m_editorScroll->value()) / editorMax;
            int targetValue = static_cast<int>(ratio * previewMax);
            m_previewScroll->setValue(targetValue);
        }
    } else if (m_mode == SyncMode::HeadingBased) {
        // 基于标题同步（简化版：按当前可见行比例）
        int editorVal = m_editorScroll->value();
        int editorMax = m_editorScroll->maximum();
        int previewMax = m_previewScroll->maximum();

        if (editorMax > 0 && previewMax > 0) {
            double ratio = static_cast<double>(editorVal) / editorMax;
            m_previewScroll->setValue(static_cast<int>(ratio * previewMax));
        }
    }

    m_syncing = false;
}

void ScrollSyncManager::onPreviewScroll() {
    if (!m_enabled || !m_previewToEditorEnabled || m_syncing || !m_editor || !m_editorScroll || !m_previewScroll) {
        return;
    }

    m_syncing = true;

    int previewMax = m_previewScroll->maximum();
    int editorMax = m_editorScroll->maximum();

    if (previewMax > 0 && editorMax > 0) {
        double ratio = static_cast<double>(m_previewScroll->value()) / previewMax;
        int targetValue = static_cast<int>(ratio * editorMax);
        m_editorScroll->setValue(targetValue);
    }

    m_syncing = false;
}

void ScrollSyncManager::syncEditorToPreview() {
    onEditorScroll();
}

void ScrollSyncManager::syncPreviewToEditor() {
    onPreviewScroll();
}

} // namespace dmc
