// ─────────────────────────────────────────────────────────────────────────────
// DocMind AI — LineNumberArea
// 行号区域（与 QPlainTextEdit 联动的左侧边栏）
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QWidget>

namespace dmc {

class MarkdownEditor;

class LineNumberArea : public QWidget {
    Q_OBJECT

public:
    explicit LineNumberArea(MarkdownEditor* editor);
    ~LineNumberArea() override;

    QSize sizeHint() const override;

    /// 设置是否显示
    void setVisible(bool visible) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    MarkdownEditor* m_editor;
};

} // namespace dmc
