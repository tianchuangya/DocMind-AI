#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include "conversion/Types.h"
#include "conversion/ConversionService.h"

using namespace dmc::conversion;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DocMind-AI");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "DocMind-AI 文档转换中枢 — 模块 B");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceOpt({"s", "source"},       "源文件路径", "file");
    QCommandLineOption outputOpt({"o", "output"},       "输出文件路径", "file");
    QCommandLineOption srcFmtOpt({"sf", "source-format"},"源格式 (md/docx/html/pdf)", "fmt");
    QCommandLineOption tgtFmtOpt({"tf", "target-format"},"目标格式 (md/docx/html/pdf)", "fmt");
    QCommandLineOption listOpt({"l", "list"},           "列出支持的转换方向");
    QCommandLineOption diagOpt({"d", "diag"},           "显示工具诊断报告");
    QCommandLineOption extractOpt({"e", "extract"},     "文本提取模式（输出到 stdout）");

    parser.addOption(sourceOpt);
    parser.addOption(outputOpt);
    parser.addOption(srcFmtOpt);
    parser.addOption(tgtFmtOpt);
    parser.addOption(listOpt);
    parser.addOption(diagOpt);
    parser.addOption(extractOpt);
    parser.process(app);

    ConversionService svc;

    // ── 诊断报告 ──
    if (parser.isSet(diagOpt)) {
        qInfo().noquote() << svc.diagnostics()->generateReport();
        return 0;
    }

    // ── 列出转换方向 ──
    if (parser.isSet(listOpt)) {
        auto dirs = svc.supportedDirections();
        qInfo() << "支持的转换方向:";
        for (auto d : dirs) {
            QString s, t;
            switch (d) {
                case ConversionDirection::MD_to_DOCX: s="Markdown"; t="DOCX"; break;
                case ConversionDirection::DOCX_to_MD: s="DOCX"; t="Markdown"; break;
                case ConversionDirection::MD_to_HTML: s="Markdown"; t="HTML"; break;
                case ConversionDirection::HTML_to_MD: s="HTML"; t="Markdown"; break;
                case ConversionDirection::MD_to_PDF:  s="Markdown"; t="PDF"; break;
                case ConversionDirection::PDF_to_MD:  s="PDF"; t="Markdown"; break;
                default: continue;
            }
            qInfo("  %s → %s", qPrintable(s), qPrintable(t));
        }
        if (dirs.isEmpty()) qInfo("  (无可用工具)");
        return 0;
    }

    // ── 文本提取 ──
    if (parser.isSet(extractOpt)) {
        if (!parser.isSet(sourceOpt)) {
            qCritical() << "提取模式需要 --source 参数";
            return 1;
        }
        QString src = parser.value(sourceOpt);
        if (!QFileInfo::exists(src)) {
            qCritical() << "源文件不存在:" << src;
            return 1;
        }

        TextExtractionRequest req;
        req.source_path = src;
        if (parser.isSet(srcFmtOpt))
            req.source_format = parser.value(srcFmtOpt);
        req.prefer_structure = true;

        QObject::connect(&svc, &ConversionService::extractionFinished,
            [&](const TextExtractionRequest&, const TextExtractionResult& result) {
                qInfo() << "提取成功:";
                qInfo() << "  块数:" << result.blocks.size();
                qInfo() << "  文本长度:" << result.markdown_text.size() << "字符";
                for (int i = 0; i < qMin(result.blocks.size(), 20); ++i) {
                    const auto& b = result.blocks[i];
                    QString tag;
                    switch (b.type) {
                        case StructBlock::Heading:    tag = QStringLiteral("H%1").arg(b.level); break;
                        case StructBlock::Paragraph:  tag = "P"; break;
                        case StructBlock::ListItem:   tag = "LI"; break;
                        case StructBlock::CodeBlock:  tag = "CODE"; break;
                        case StructBlock::TableCell:  tag = "TD"; break;
                        case StructBlock::Blockquote: tag = "BQ"; break;
                    }
                    QString loc;
                    if (b.sourcePage >= 0) loc += QStringLiteral(" p%1").arg(b.sourcePage);
                    if (b.sourceLine >= 0) loc += QStringLiteral(" l%1").arg(b.sourceLine);
                    qInfo("  [%s]%s %s",
                          qPrintable(tag), qPrintable(loc),
                          qPrintable(b.text.left(60)));
                }
                if (result.blocks.size() > 20)
                    qInfo("  ... +%lld more", (long long)(result.blocks.size() - 20));
                app.quit();
            });

        QObject::connect(&svc, &ConversionService::extractionFailed,
            [&](const TextExtractionRequest&, ConversionError code, const QString& msg) {
                qCritical() << "提取失败:" << msg << "(" + errorToString(code) + ")";
                app.exit(1);
            });

        svc.extractTextAsync(req);
        return app.exec();
    }

    // ── 文件转换 ──
    if (parser.isSet(sourceOpt) && parser.isSet(outputOpt) &&
        parser.isSet(srcFmtOpt) && parser.isSet(tgtFmtOpt)) {

        QString src = parser.value(sourceOpt);
        QString dst = parser.value(outputOpt);
        Format sf   = formatFromString(parser.value(srcFmtOpt));
        Format tf   = formatFromString(parser.value(tgtFmtOpt));

        if (sf == Format::Unknown || tf == Format::Unknown) {
            qCritical() << "不支持的格式";
            return 1;
        }
        if (!QFileInfo::exists(src)) {
            qCritical() << "源文件不存在:" << src;
            return 1;
        }
        if (!svc.canConvert(sf, tf)) {
            qCritical() << "不支持" << formatToString(sf) << "→" << formatToString(tf);
            return 1;
        }

        TaskInput input;
        input.source_path       = src;
        input.source_format     = sf;
        input.target_format     = tf;
        input.output_path       = dst;
        input.overwrite_existing = true;

        QObject::connect(&svc, &ConversionService::taskCompleted,
            [&](TaskHandle, const TaskOutput& o) {
                qInfo() << "✓ 转换完成！";
                if (o.product_path) qInfo() << "  输出:" << *o.product_path;
                qInfo() << "  耗时:" << o.duration_ms << "ms";
                app.quit();
            });

        QObject::connect(&svc, &ConversionService::taskFailed,
            [&](TaskHandle, const TaskOutput& o) {
                qCritical() << "✗ 转换失败:";
                if (o.error_message) qCritical() << "  原因:" << *o.error_message;
                qCritical() << "  错误码:" << errorToString(o.error_code);
                app.exit(1);
            });

        qInfo() << "开始转换:" << formatToString(sf) << "→" << formatToString(tf);
        qInfo() << "  源:" << src;
        qInfo() << "  输出:" << dst;

        svc.submitConversion(input);
        return app.exec();
    }

    // 没有有效参数 → 显示帮助
    parser.showHelp(0);
    return 0;
}
