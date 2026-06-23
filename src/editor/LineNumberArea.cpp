// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — LineNumberArea 实现
// ─────────────────────────────────────────────────────────────────────────────
#include "editor/LineNumberArea.h"
#include "editor/MarkdownEditor.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QTextBlock>
#include <QPalette>

namespace dmc {

LineNumberArea::LineNumberArea(MarkdownEditor* editor)
    : QWidget(editor)
    , m_editor(editor)
{
    setMouseTracking(true);
}

LineNumberArea::~LineNumberArea() = default;

QSize LineNumberArea::sizeHint() const {
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::setVisible(bool visible) {
    QWidget::setVisible(visible);
    m_editor->updateLineNumberAreaWidth();
}

void LineNumberArea::paintEvent(QPaintEvent* event) {
    m_editor->paintLineNumberArea(this, event);
}

void LineNumberArea::mousePressEvent(QMouseEvent* event) {
    // 点击行号区域 → 选中整行
    if (event->button() == Qt::LeftButton) {
        QTextBlock block = m_editor->document()->findBlock(event->position().toPoint().y());
        if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.select(QTextCursor::BlockUnderCursor);
            m_editor->setTextCursor(cursor);
        }
    }
    QWidget::mousePressEvent(event);
}

} // namespace dmc
