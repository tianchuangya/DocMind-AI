#include "NativePdfConverter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QFont>
#include <QPdfWriter>
#include <QPagedPaintDevice>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QMarginsF>
#include <QPainter>
#include <zlib.h>
#include <QMap>

namespace dmc {
namespace conversion {

NativePdfConverter::NativePdfConverter(QObject* parent) : QObject(parent) {}
NativePdfConverter::~NativePdfConverter() {}

// ─── zlib 解压 ──────────────────────────────────────────

static QByteArray zlibDecompress(const QByteArray& data) {
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    strm.avail_in = static_cast<uInt>(data.size());
    if (inflateInit2(&strm, 15 + 32) != Z_OK) return {};

    QByteArray result;
    result.resize(qMax(data.size() * 4, 4096));
    strm.next_out  = reinterpret_cast<Bytef*>(result.data());
    strm.avail_out = static_cast<uInt>(result.size());

    int ret;
    while ((ret = inflate(&strm, Z_NO_FLUSH)) == Z_OK) {
        if (strm.avail_out == 0) {
            int old = result.size();
            result.resize(old * 2);
            strm.next_out  = reinterpret_cast<Bytef*>(result.data() + old);
            strm.avail_out = static_cast<uInt>(old);
        }
    }
    result.resize(result.size() - strm.avail_out);
    inflateEnd(&strm);
    return (ret == Z_STREAM_END || ret == Z_OK) ? result : QByteArray();
}

// ─── 解析 ToUnicode CMap ────────────────────────────────
// 支持 beginbfchar 和 beginbfrange

static QMap<int, QChar> parseCMap(const QByteArray& cmap_data) {
    QMap<int, QChar> map;
    QString text = QString::fromLatin1(cmap_data);

    // beginbfchar: <src> <dst> 逐对
    QRegularExpression bfchar_re("beginbfchar\\s*(.*?)\\s*endbfchar",
                                  QRegularExpression::DotMatchesEverythingOption);
    auto bc_it = bfchar_re.globalMatch(text);
    while (bc_it.hasNext()) {
        auto m = bc_it.next();
        QRegularExpression pair_re("<([0-9a-fA-F]+)>\\s*<([0-9a-fA-F]+)>");
        auto p_it = pair_re.globalMatch(m.captured(1));
        while (p_it.hasNext()) {
            auto p = p_it.next();
            bool ok1, ok2;
            int src = p.captured(1).toInt(&ok1, 16);
            int dst = p.captured(2).toInt(&ok2, 16);
            if (ok1 && ok2 && dst <= 0xFFFF) map[src] = QChar(dst);
        }
    }

    // beginbfrange 解析
    QRegularExpression bfrange_re("beginbfrange\\s*(.*?)\\s*endbfrange",
                                   QRegularExpression::DotMatchesEverythingOption);
    auto br_it = bfrange_re.globalMatch(text);
    while (br_it.hasNext()) {
        auto m = br_it.next();
        QString block = m.captured(1);

        // 逐行处理 bfrange 条目
        QStringList entries = block.split('\n', Qt::SkipEmptyParts);
        for (const QString& entry : entries) {
            QString e = entry.trimmed();
            if (e.isEmpty()) continue;

            // 带数组: <lo> <hi> [<v1> <v2> ...]
            QRegularExpression arr_re(
                "^<([0-9a-fA-F]+)>\\s*<([0-9a-fA-F]+)>\\s*\\[(.*)\\]$",
                QRegularExpression::DotMatchesEverythingOption);
            auto arr_m = arr_re.match(e);
            if (arr_m.hasMatch()) {
                bool ok1, ok2;
                int lo = arr_m.captured(1).toInt(&ok1, 16);
                int hi = arr_m.captured(2).toInt(&ok2, 16);
                if (!ok1 || !ok2) continue;

                QRegularExpression val_re("<([0-9a-fA-F]+)>");
                auto v_it = val_re.globalMatch(arr_m.captured(3));
                int idx = 0;
                while (v_it.hasNext()) {
                    auto v = v_it.next();
                    bool ok3;
                    int code = v.captured(1).toInt(&ok3, 16);
                    if (ok3 && code <= 0xFFFF && lo + idx <= hi)
                        map[lo + idx] = QChar(code);
                    idx++;
                }
                continue; // 已处理，不进入下面的单值匹配
            }

            // 单值: <lo> <hi> <dst_start>（不含 [...]）
            if (!e.contains('[')) {
                QRegularExpression single_re(
                    "^<([0-9a-fA-F]+)>\\s*<([0-9a-fA-F]+)>\\s*<([0-9a-fA-F]+)>$");
                auto s_m = single_re.match(e);
                if (s_m.hasMatch()) {
                    bool ok1, ok2, ok3;
                    int lo  = s_m.captured(1).toInt(&ok1, 16);
                    int hi  = s_m.captured(2).toInt(&ok2, 16);
                    int dst = s_m.captured(3).toInt(&ok3, 16);
                    if (ok1 && ok2 && ok3 && dst <= 0xFFFF) {
                        for (int i = lo; i <= hi; ++i) {
                            int code = dst + (i - lo);
                            if (code <= 0xFFFF)
                                map[i] = QChar(code);
                        }
                    }
                }
            }
        }
    }

    return map;
}

// ─── 提取所有 PDF 流 ───────────────────────────────────

struct PdfStream {
    QByteArray raw;
    QByteArray decompressed;
    bool is_compressed{false};
};

static QVector<PdfStream> extractAllStreams(const QByteArray& pdf_data) {
    QVector<PdfStream> streams;
    QByteArray sd1 = "stream\r\n", sd2 = "stream\n", ed = "endstream";
    int pos = 0;

    while (pos < pdf_data.size()) {
        int start = pdf_data.indexOf(sd1, pos);
        int off = sd1.size();
        if (start < 0) { start = pdf_data.indexOf(sd2, pos); off = sd2.size(); }
        if (start < 0) break;

        start += off;
        int end = pdf_data.indexOf(ed, start);
        if (end < 0) break;

        PdfStream ps;
        ps.raw = pdf_data.mid(start, end - start);
        while (ps.raw.endsWith('\n') || ps.raw.endsWith('\r')) ps.raw.chop(1);

        ps.decompressed = zlibDecompress(ps.raw);
        ps.is_compressed = !ps.decompressed.isEmpty();
        if (!ps.is_compressed) ps.decompressed = ps.raw;

        streams.append(ps);
        pos = end + ed.size();
    }
    return streams;
}

// ─── 查找字体对应的 CMap ────────────────────────────────
// 通过 PDF 对象引用关系匹配字体名 → CMap

static QMap<QString, QMap<int, QChar>> buildFontCMaps(const QByteArray& pdf_data,
                                                       const QVector<PdfStream>& /*streams*/) {
    QMap<QString, QMap<int, QChar>> font_maps;
    QString pdf_text = QString::fromLatin1(pdf_data);

    // ── 步骤 1：找所有 PDF 对象的 (obj_num, start_pos, end_pos) ──
    struct PdfObj { int num; int start; int end; };
    QVector<PdfObj> objects;
    QRegularExpression obj_start_re("(\\d+)\\s+\\d+\\s+obj\\b");
    auto os_it = obj_start_re.globalMatch(pdf_text);
    QVector<QPair<int,int>> obj_starts; // (pos, obj_num)
    while (os_it.hasNext()) {
        auto m = os_it.next();
        obj_starts.append({m.capturedStart(), m.captured(1).toInt()});
    }
    for (int i = 0; i < obj_starts.size(); ++i) {
        PdfObj o;
        o.num   = obj_starts[i].second;
        o.start = obj_starts[i].first;
        o.end   = (i + 1 < obj_starts.size()) ? obj_starts[i + 1].first : pdf_data.size();
        objects.append(o);
    }

    // ── 步骤 2：找到每个对象内的 CMap 流并解析 ──
    // 一个对象可能包含 stream...endstream
    QMap<int, QMap<int, QChar>> obj_to_cmap; // obj_num → CMap
    QByteArray sd1 = "stream\r\n", sd2 = "stream\n", ed = "endstream";

    for (const auto& obj : objects) {
        QByteArray obj_data = pdf_data.mid(obj.start, obj.end - obj.start);
        int s_pos = obj_data.indexOf(sd1);
        int s_off = sd1.size();
        if (s_pos < 0) { s_pos = obj_data.indexOf(sd2); s_off = sd2.size(); }
        if (s_pos < 0) continue;

        s_pos += s_off;
        int e_pos = obj_data.indexOf(ed, s_pos);
        if (e_pos < 0) continue;

        QByteArray stream_raw = obj_data.mid(s_pos, e_pos - s_pos);
        while (stream_raw.endsWith('\n') || stream_raw.endsWith('\r'))
            stream_raw.chop(1);

        QByteArray decompressed = zlibDecompress(stream_raw);
        if (decompressed.isEmpty()) decompressed = stream_raw;

        QString text = QString::fromLatin1(decompressed);
        if (text.contains("beginbfrange") || text.contains("beginbfchar")) {
            obj_to_cmap[obj.num] = parseCMap(decompressed);
        }
    }

    // ── 步骤 3：字体名 → 字体对象号（从 /Font << >> 资源字典）──
    QMap<QString, int> font_name_to_obj;
    QRegularExpression fontres_re("/Font\\s*<<(.*?)>>",
                                   QRegularExpression::DotMatchesEverythingOption);
    auto fr_it = fontres_re.globalMatch(pdf_text);
    while (fr_it.hasNext()) {
        auto m = fr_it.next();
        QRegularExpression fm_re("/(F\\d+)\\s+(\\d+)\\s+\\d+\\s+R");
        auto fm_it = fm_re.globalMatch(m.captured(1));
        while (fm_it.hasNext()) {
            auto fm = fm_it.next();
            font_name_to_obj["/" + fm.captured(1)] = fm.captured(2).toInt();
        }
    }

    // ── 步骤 4：字体对象号 → ToUnicode 对象号 ──
    QMap<int, int> font_obj_to_tu;
    for (const auto& obj : objects) {
        QString obj_text = pdf_text.mid(obj.start, obj.end - obj.start);
        // 只匹配 Type0 字体（包含 /Type /Font 和 /ToUnicode）
        if (obj_text.contains("/Type /Font") || obj_text.contains("/Type/Font")) {
            QRegularExpression tu_re("/ToUnicode\\s+(\\d+)\\s+\\d+\\s+R");
            auto tu_m = tu_re.match(obj_text);
            if (tu_m.hasMatch())
                font_obj_to_tu[obj.num] = tu_m.captured(1).toInt();
        }
    }

    // ── 步骤 5：组合 → 字体名 → CMap ──
    for (auto it = font_name_to_obj.cbegin(); it != font_name_to_obj.cend(); ++it) {
        int font_obj = it.value();
        if (font_obj_to_tu.contains(font_obj)) {
            int tu_obj = font_obj_to_tu[font_obj];
            if (obj_to_cmap.contains(tu_obj)) {
                font_maps[it.key()] = obj_to_cmap[tu_obj];
            }
        }
    }

    return font_maps;
}

// ─── 从内容流解码文本 ───────────────────────────────────

static QString decodeContentStream(const QString& content,
                                    const QMap<QString, QMap<int, QChar>>& font_maps) {
    QString result;
    QString current_font;
    bool in_bt = false;

    QStringList lines = content.split('\n');
    for (const QString& line : lines) {
        QString t = line.trimmed();

        // 字体切换: /F12 22 Tf
        QRegularExpression tf_re("/(F\\d+)\\s+(\\d+)\\s+Tf");
        auto tf_m = tf_re.match(t);
        if (tf_m.hasMatch()) {
            current_font = "/" + tf_m.captured(1);
        }

        // BT...ET 文本块
        if (t == "BT") { in_bt = true; continue; }
        if (t == "ET") {
            if (in_bt) result += '\n';
            in_bt = false;
            continue;
        }

        if (!in_bt) continue;

        // 水平移动 (Td) 超过阈值 → 可能是新段落/新行
        QRegularExpression td_re("^([\\d.]+)\\s+([\\d.-]+)\\s+Td");
        auto td_m = td_re.match(t);
        if (td_m.hasMatch()) {
            double dy = td_m.captured(2).toDouble();
            if (dy < 0) result += '\n'; // 向下移动 = 新行
        }

        // <hex> Tj — 单个字形
        QRegularExpression hex_tj_re("<([0-9a-fA-F]+)>\\s*Tj");
        auto hj_it = hex_tj_re.globalMatch(t);
        while (hj_it.hasNext()) {
            auto m = hj_it.next();
            bool ok;
            int gid = m.captured(1).toInt(&ok, 16);
            if (!ok) continue;

            auto fm = font_maps.value(current_font);
            if (fm.contains(gid))
                result += fm[gid];
        }

        // [(<hex>) kern (<hex>)] TJ — 字形数组
        QRegularExpression arr_tj_re("\\[(.*?)\\]\\s*TJ");
        auto aj_it = arr_tj_re.globalMatch(t);
        while (aj_it.hasNext()) {
            auto m = aj_it.next();
            QRegularExpression hex_re("<([0-9a-fA-F]+)>");
            auto h_it = hex_re.globalMatch(m.captured(1));
            while (h_it.hasNext()) {
                auto hm = h_it.next();
                bool ok;
                int gid = hm.captured(1).toInt(&ok, 16);
                if (!ok) continue;
                auto fm = font_maps.value(current_font);
                if (fm.contains(gid)) result += fm[gid];
            }
        }

        // (text) Tj — 直接文本（少数 PDF 用这种）
        QRegularExpression str_tj_re("\\(([^)]*)\\)\\s*Tj");
        auto sj_it = str_tj_re.globalMatch(t);
        while (sj_it.hasNext()) result += sj_it.next().captured(1);
    }

    return result;
}

// ─── 完整提取流程 ───────────────────────────────────────

static QString extractTextFromPdf(const QByteArray& pdf_data) {
    auto streams = extractAllStreams(pdf_data);
    auto font_maps = buildFontCMaps(pdf_data, streams);

    QString all_text;
    for (const auto& ps : streams) {
        QString content = QString::fromLatin1(ps.decompressed);
        // 内容流包含 BT/ET 操作符
        if (content.contains("BT") && content.contains("ET") &&
            (content.contains("Tj") || content.contains("TJ"))) {
            all_text += decodeContentStream(content, font_maps);
        }
    }

    return all_text;
}

// ─── exportToPdf ────────────────────────────────────────

TaskOutput NativePdfConverter::exportToPdf(const TaskInput& input) {
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

    QString html = markdownToHtml(markdown);

    QTextDocument doc;
    doc.setHtml(html);
    doc.setDefaultFont(QFont("Helvetica", 11));

    QPdfWriter writer(input.output_path);
    writer.setPageSize(m_page_size);
    writer.setPageMargins(QMarginsF(m_margin_left, m_margin_top,
                                     m_margin_right, m_margin_bottom));
    writer.setTitle(QFileInfo(input.source_path).baseName());
    writer.setCreator("DocMind-AI");

    QPainter painter(&writer);
    if (!painter.isActive()) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::Unknown;
        output.error_message = QStringLiteral("PDF 渲染失败");
        return output;
    }
    doc.drawContents(&painter);
    painter.end();

    output.status       = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms  = timer.elapsed();
    output.logs << QStringLiteral("Markdown → PDF 转换成功（原生实现）");
    return output;
}

// ─── importFromPdf ──────────────────────────────────────

TaskOutput NativePdfConverter::importFromPdf(const TaskInput& input) {
    TaskOutput output;
    output.status = TaskStatus::Running;

    QElapsedTimer timer;
    timer.start();

    QFileInfo fi(input.source_path);
    if (!fi.exists()) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::SourceNotFound;
        output.error_message = QStringLiteral("PDF 文件不存在: ") + input.source_path;
        return output;
    }

    QFile pdf_file(input.source_path);
    if (!pdf_file.open(QIODevice::ReadOnly)) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::SourceNotFound;
        output.error_message = QStringLiteral("无法打开 PDF 文件");
        return output;
    }
    QByteArray pdf_data = pdf_file.readAll();
    pdf_file.close();

    QString extracted = extractTextFromPdf(pdf_data);

    // 清理多余空行
    QStringList lines = extracted.split('\n');
    QString merged;
    int consecutive_empty = 0;
    for (const QString& line : lines) {
        QString t = line.trimmed();
        if (t.isEmpty()) {
            consecutive_empty++;
            if (consecutive_empty <= 1) merged += '\n';
            continue;
        }
        consecutive_empty = 0;
        merged += t + '\n';
    }

    if (merged.trimmed().isEmpty()) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::ScannedPdfNoOcr;
        output.error_message = QStringLiteral("无法提取 PDF 文本，建议安装 Poppler");
        return output;
    }

    QFileInfo out_info(input.output_path);
    QDir(out_info.dir()).mkpath(".");

    QFile out(input.output_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        output.status       = TaskStatus::Failed;
        output.error_code   = ConversionError::Unknown;
        output.error_message = QStringLiteral("无法写入输出文件: ") + input.output_path;
        return output;
    }
    out.write(merged.toUtf8());
    out.close();

    output.status       = TaskStatus::Completed;
    output.product_path = input.output_path;
    output.duration_ms  = timer.elapsed();
    output.logs << QStringLiteral("PDF → Markdown 转换成功（原生 CMap + zlib 提取）");
    return output;
}

void NativePdfConverter::setMargins(int top, int right, int bottom, int left) {
    m_margin_top = top; m_margin_right = right;
    m_margin_bottom = bottom; m_margin_left = left;
}

QString NativePdfConverter::markdownToHtml(const QString& markdown) const {
    QString html;
    QStringList lines = markdown.split('\n');
    bool in_code = false, in_p = false;

    for (const QString& line : lines) {
        QString t = line.trimmed();
        if (t.startsWith("```")) {
            if (in_code) { html += "</code></pre>\n"; in_code = false; }
            else { if (in_p) { html += "</p>\n"; in_p = false; }
                   html += "<pre><code>"; in_code = true; }
            continue;
        }
        if (in_code) { html += line.toHtmlEscaped() + '\n'; continue; }
        if (t.isEmpty()) { if (in_p) { html += "</p>\n"; in_p = false; } continue; }

        if (t.startsWith("# ")) {
            if (in_p) { html += "</p>\n"; in_p = false; }
            html += "<h1>" + processInlineMarkdown(t.mid(2)) + "</h1>\n"; continue;
        }
        if (t.startsWith("## ")) {
            if (in_p) { html += "</p>\n"; in_p = false; }
            html += "<h2>" + processInlineMarkdown(t.mid(3)) + "</h2>\n"; continue;
        }
        if (t.startsWith("### ")) {
            if (in_p) { html += "</p>\n"; in_p = false; }
            html += "<h3>" + processInlineMarkdown(t.mid(4)) + "</h3>\n"; continue;
        }
        if (t.startsWith("- ") || t.startsWith("* ")) {
            if (in_p) { html += "</p>\n"; in_p = false; }
            html += "<p>\xe2\x80\xa2 " + processInlineMarkdown(t.mid(2)) + "</p>\n"; continue;
        }
        if (t.startsWith("> ")) {
            if (in_p) { html += "</p>\n"; in_p = false; }
            html += "<p><i>" + processInlineMarkdown(t.mid(2)) + "</i></p>\n"; continue;
        }
        if (!in_p) { html += "<p>"; in_p = true; } else { html += ' '; }
        html += processInlineMarkdown(t);
    }
    if (in_p) html += "</p>\n";
    return html;
}

QString NativePdfConverter::processInlineMarkdown(const QString& text) const {
    QString r = text.toHtmlEscaped();
    r.replace(QRegularExpression("`([^`]+)`"),         "<code>\\1</code>");
    r.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<strong>\\1</strong>");
    r.replace(QRegularExpression("__([^_]+)__"),        "<strong>\\1</strong>");
    r.replace(QRegularExpression("\\*([^*]+)\\*"),      "<em>\\1</em>");
    r.replace(QRegularExpression("_([^_]+)_"),          "<em>\\1</em>");
    r.replace(QRegularExpression("~~([^~]+)~~"),        "<del>\\1</del>");
    return r;
}

} // namespace conversion
} // namespace dmc
