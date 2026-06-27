#include "NativeDocxConverter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QBuffer>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>

namespace dmc {
namespace conversion {

namespace {

bool writePackageFile(const QString& root, const QString& relativePath, const QByteArray& data) {
    const QString path = QDir(root).filePath(relativePath);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

bool runProcess(const QString& program,
                const QStringList& arguments,
                const QString& workingDirectory,
                QString* error) {
    QProcess process;
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }
    process.start(program, arguments);
    if (!process.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("无法启动 %1").arg(program);
        }
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        if (error) {
            *error = QStringLiteral("%1 执行超时").arg(program);
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *error = stderrText.isEmpty()
                ? QStringLiteral("%1 执行失败，退出码 %2").arg(program).arg(process.exitCode())
                : stderrText;
        }
        return false;
    }
    return true;
}

} // namespace

NativeDocxConverter::NativeDocxConverter(QObject* parent) : QObject(parent) {}
NativeDocxConverter::~NativeDocxConverter() {}

TaskOutput NativeDocxConverter::exportToDocx(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;

    QElapsedTimer timer;
    timer.start();

    QFile source_file(input.source_path);
    if (!source_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::SourceNotFound;
        output.error_message = QStringLiteral("无法打开源文件: ") + input.source_path;
        return output;
    }

    QString markdown = QString::fromUtf8(source_file.readAll());
    source_file.close();

    DocxDocument doc;
    doc.content_xml = markdownToOoxml(markdown);

    if (!buildDocxFile(input.output_path, doc)) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::Unknown;
        output.error_message = QStringLiteral("无法创建 DOCX 文件");
        return output;
    }

    output.status       = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms  = timer.elapsed();
    output.logs << QStringLiteral("Markdown → DOCX 转换成功（原生实现）");
    return output;
}

TaskOutput NativeDocxConverter::importFromDocx(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;

    QElapsedTimer timer;
    timer.start();

    QString error;
    DocxDocument doc = parseDocxFile(input.source_path, error);

    if (!error.isEmpty()) {
        output.status = TaskStatus::Failed;
        if (error.contains("password") || error.contains("encrypted"))
            output.error_code = ConversionError::PasswordProtected;
        else if (error.contains("corrupt") || error.contains("invalid"))
            output.error_code = ConversionError::CorruptFile;
        else
            output.error_code = ConversionError::SourceNotFound;
        output.error_message = error;
        return output;
    }

    QString markdown = ooxmlToMarkdown(doc.content_xml);

    QFile output_file(input.output_path);
    if (!output_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::Unknown;
        output.error_message = QStringLiteral("无法写入输出文件: ") + input.output_path;
        return output;
    }
    output_file.write(markdown.toUtf8());
    output_file.close();

    output.status       = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms  = timer.elapsed();
    output.logs << QStringLiteral("DOCX → Markdown 转换成功（原生实现）");
    return output;
}

bool NativeDocxConverter::buildDocxFile(const QString& output_path,
                                        const DocxDocument& doc) {
    QTemporaryDir temp_dir;
    if (!temp_dir.isValid()) {
        return false;
    }

    const QString root = temp_dir.path();
    if (!writePackageFile(root, "[Content_Types].xml", generateContentTypes(doc)) ||
        !writePackageFile(root, "_rels/.rels", generateRels()) ||
        !writePackageFile(root, "word/_rels/document.xml.rels", generateWordRels(doc)) ||
        !writePackageFile(root, "word/styles.xml", generateStylesXml()) ||
        !writePackageFile(root, "word/document.xml", generateDocumentXml(doc))) {
        return false;
    }

    for (auto it = doc.images.cbegin(); it != doc.images.cend(); ++it) {
        if (!writePackageFile(root, "word/media/" + it.key(), it.value())) {
            return false;
        }
    }

    QDir().mkpath(QFileInfo(output_path).absolutePath());
    QFile::remove(output_path);

    QString error;
    return runProcess(QStringLiteral("zip"),
                      QStringList{QStringLiteral("-qr"), output_path, QStringLiteral(".")},
                      root,
                      &error);
}

NativeDocxConverter::DocxDocument
NativeDocxConverter::parseDocxFile(const QString& input_path, QString& error) {
    DocxDocument doc;

    if (!QFileInfo::exists(input_path)) {
        error = QStringLiteral("文件不存在或无法访问");
        return doc;
    }

    QTemporaryDir temp_dir;
    if (!temp_dir.isValid()) {
        error = QStringLiteral("无法创建临时目录");
        return doc;
    }

    if (!runProcess(QStringLiteral("unzip"),
                    QStringList{QStringLiteral("-qq"), QStringLiteral("-o"), input_path,
                                QStringLiteral("-d"), temp_dir.path()},
                    QString(),
                    &error)) {
        if (error.isEmpty()) {
            error = QStringLiteral("无效的 DOCX 文件");
        }
        return doc;
    }

    QFile document_file(QDir(temp_dir.path()).filePath("word/document.xml"));
    if (!document_file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("DOCX 文件中缺少 document.xml");
        return doc;
    }

    QByteArray doc_xml = document_file.readAll();
    if (doc_xml.isEmpty()) {
        error = QStringLiteral("DOCX 文件中缺少 document.xml");
        return doc;
    }
    doc.content_xml = QString::fromUtf8(doc_xml);

    QDir media_dir(QDir(temp_dir.path()).filePath("word/media"));
    const QFileInfoList media_files = media_dir.entryInfoList(QDir::Files);
    for (const QFileInfo& media : media_files) {
        QFile file(media.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            doc.images[media.fileName()] = file.readAll();
        }
    }

    return doc;
}

QString NativeDocxConverter::markdownToOoxml(const QString& markdown) const {
    QString ooxml;
    QStringList lines = markdown.split('\n');
    bool in_code_block = false;

    for (const QString& line : lines) {
        QString t = line.trimmed();

        if (t.startsWith("```")) { in_code_block = !in_code_block; continue; }
        if (in_code_block) {
            ooxml += "<w:p><w:pPr><w:pStyle w:val=\"Code\"/></w:pPr>"
                     "<w:r><w:t xml:space=\"preserve\">" + escapeXml(line) + "</w:t></w:r></w:p>";
            continue;
        }
        if (t.isEmpty()) continue;

        if (t.startsWith("# ")) {
            ooxml += "<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>"
                     "<w:r><w:t>" + escapeXml(t.mid(2)) + "</w:t></w:r></w:p>";
        } else if (t.startsWith("## ")) {
            ooxml += "<w:p><w:pPr><w:pStyle w:val=\"Heading2\"/></w:pPr>"
                     "<w:r><w:t>" + escapeXml(t.mid(3)) + "</w:t></w:r></w:p>";
        } else if (t.startsWith("### ")) {
            ooxml += "<w:p><w:pPr><w:pStyle w:val=\"Heading3\"/></w:pPr>"
                     "<w:r><w:t>" + escapeXml(t.mid(4)) + "</w:t></w:r></w:p>";
        } else if (t.startsWith("- ") || t.startsWith("* ")) {
            ooxml += "<w:p><w:pPr><w:pStyle w:val=\"ListBullet\"/></w:pPr>"
                     "<w:r><w:t>" + escapeXml(t.mid(2)) + "</w:t></w:r></w:p>";
        } else if (t.startsWith("> ")) {
            ooxml += "<w:p><w:pPr><w:pStyle w:val=\"Quote\"/></w:pPr>"
                     "<w:r><w:t>" + escapeXml(t.mid(2)) + "</w:t></w:r></w:p>";
        } else {
            ooxml += "<w:p><w:r><w:t xml:space=\"preserve\">" + escapeXml(t) + "</w:t></w:r></w:p>";
        }
    }
    return ooxml;
}

QString NativeDocxConverter::ooxmlToMarkdown(const QString& ooxml) const {
    QString markdown;
    QRegularExpression p_re("<w:p[^>]*>(.*?)</w:p>",
                            QRegularExpression::DotMatchesEverythingOption);
    auto it = p_re.globalMatch(ooxml);

    while (it.hasNext()) {
        auto match = it.next();
        QString para = match.captured(1);

        QString style;
        QRegularExpression style_re("<w:pStyle w:val=\"([^\"]+)\"");
        auto sm = style_re.match(para);
        if (sm.hasMatch()) style = sm.captured(1);

        QString text;
        QRegularExpression t_re("<w:t[^>]*>(.*?)</w:t>",
                                QRegularExpression::DotMatchesEverythingOption);
        auto ti = t_re.globalMatch(para);
        while (ti.hasNext()) text += ti.next().captured(1);

        if (text.isEmpty()) { markdown += "\n"; continue; }

        if      (style == "Heading1")   markdown += "# " + text + "\n\n";
        else if (style == "Heading2")   markdown += "## " + text + "\n\n";
        else if (style == "Heading3")   markdown += "### " + text + "\n\n";
        else if (style == "ListBullet") markdown += "- " + text + "\n";
        else if (style == "ListNumber") markdown += "1. " + text + "\n";
        else if (style == "Quote")      markdown += "> " + text + "\n";
        else if (style == "Code")       markdown += "```\n" + text + "\n```\n";
        else                            markdown += text + "\n\n";
    }

    markdown.replace(QRegularExpression("\n{3,}"), "\n\n");
    return markdown.trimmed();
}

QByteArray NativeDocxConverter::generateContentTypes(const DocxDocument& doc) const {
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n";
    xml += "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n";
    xml += "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n";
    xml += "  <Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>\n";

    for (auto it = doc.images.cbegin(); it != doc.images.cend(); ++it) {
        QString ext = QFileInfo(it.key()).suffix().toLower();
        if (ext == "png")
            xml += "  <Default Extension=\"png\" ContentType=\"image/png\"/>\n";
        else if (ext == "jpg" || ext == "jpeg")
            xml += "  <Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>\n";
        break;
    }

    xml += "</Types>";
    return xml.toUtf8();
}

QByteArray NativeDocxConverter::generateRels() const {
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
    xml += "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>\n";
    xml += "</Relationships>";
    return xml.toUtf8();
}

QByteArray NativeDocxConverter::generateWordRels(const DocxDocument& doc) const {
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
    xml += "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n";

    int id = 2;
    for (auto it = doc.images.cbegin(); it != doc.images.cend(); ++it) {
        xml += QString("  <Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/%2\"/>\n")
                   .arg(id++).arg(it.key());
    }

    xml += "</Relationships>";
    return xml.toUtf8();
}

QByteArray NativeDocxConverter::generateStylesXml() const {
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n";
    xml += "  <w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/></w:style>\n";
    xml += "  <w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading 2\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/></w:style>\n";
    xml += "  <w:style w:type=\"paragraph\" w:styleId=\"Heading3\"><w:name w:val=\"heading 3\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/></w:style>\n";
    xml += "  <w:style w:type=\"paragraph\" w:styleId=\"ListBullet\"><w:name w:val=\"List Bullet\"/><w:basedOn w:val=\"Normal\"/></w:style>\n";
    xml += "  <w:style w:type=\"paragraph\" w:styleId=\"Quote\"><w:name w:val=\"Quote\"/><w:basedOn w:val=\"Normal\"/></w:style>\n";
    xml += "  <w:style w:type=\"paragraph\" w:styleId=\"Code\"><w:name w:val=\"Code\"/><w:basedOn w:val=\"Normal\"/></w:style>\n";
    xml += "</w:styles>";
    return xml.toUtf8();
}

QByteArray NativeDocxConverter::generateDocumentXml(const DocxDocument& doc) const {
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"\n";
    xml += "            xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n";
    xml += "  <w:body>\n";
    xml += doc.content_xml;
    xml += "\n    <w:sectPr>\n";
    xml += "      <w:pgSz w:w=\"11906\" w:h=\"16838\"/>\n";
    xml += "      <w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/>\n";
    xml += "    </w:sectPr>\n";
    xml += "  </w:body>\n";
    xml += "</w:document>";
    return xml.toUtf8();
}

QString NativeDocxConverter::escapeXml(const QString& text) const {
    QString r = text;
    r.replace("&", "&amp;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    r.replace("\"", "&quot;");
    r.replace("'", "&apos;");
    return r;
}

} // namespace conversion
} // namespace dmc
