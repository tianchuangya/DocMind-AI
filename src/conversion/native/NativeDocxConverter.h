#pragma once

#include "../Types.h"
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>

namespace dmc {
namespace conversion {

// 原生 DOCX 转换器 - 使用 Qt ZIP + OOXML 构建
class NativeDocxConverter : public QObject {
    Q_OBJECT
public:
    explicit NativeDocxConverter(QObject* parent = nullptr);
    ~NativeDocxConverter() override;

    // Markdown → DOCX
    TaskOutput exportToDocx(const TaskInput& input);

    // DOCX → Markdown
    TaskOutput importFromDocx(const TaskInput& input);

private:
    // DOCX 文件结构
    struct DocxDocument {
        QString content_xml;      // word/document.xml
        QMap<QString, QByteArray> images;  // 图片资源
        QMap<QString, QString> relationships;
    };

    // 构建 DOCX 文件
    bool buildDocxFile(const QString& output_path, const DocxDocument& doc);

    // 解析 DOCX 文件
    DocxDocument parseDocxFile(const QString& input_path, QString& error);

    // Markdown → OOXML 内容
    QString markdownToOoxml(const QString& markdown) const;

    // OOXML → Markdown
    QString ooxmlToMarkdown(const QString& ooxml) const;

    // 生成 [Content_Types].xml
    QByteArray generateContentTypes(const DocxDocument& doc) const;

    // 生成 _rels/.rels
    QByteArray generateRels() const;

    // 生成 word/_rels/document.xml.rels
    QByteArray generateWordRels(const DocxDocument& doc) const;

    // 生成 word/styles.xml
    QByteArray generateStylesXml() const;

    // 生成 word/document.xml
    QByteArray generateDocumentXml(const DocxDocument& doc) const;

    // 处理文本转义
    QString escapeXml(const QString& text) const;
};

} // namespace conversion
} // namespace dmc
