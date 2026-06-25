#include "Types.h"

namespace dmc {
namespace conversion {

QString errorToString(ConversionError e) {
    switch (e) {
        case ConversionError::None:              return QStringLiteral("none");
        case ConversionError::ToolMissing:       return QStringLiteral("tool_missing");
        case ConversionError::SourceNotFound:    return QStringLiteral("source_not_found");
        case ConversionError::UnsupportedFormat: return QStringLiteral("unsupported_format");
        case ConversionError::CorruptFile:       return QStringLiteral("corrupt_file");
        case ConversionError::PasswordProtected: return QStringLiteral("password_protected");
        case ConversionError::ScannedPdfNoOcr:   return QStringLiteral("scanned_pdf_no_ocr");
        case ConversionError::Timeout:           return QStringLiteral("timeout");
        case ConversionError::Cancelled:         return QStringLiteral("cancelled");
        case ConversionError::Unknown:           return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString errorToUserMessage(ConversionError e) {
    switch (e) {
        case ConversionError::None:              return QStringLiteral("无错误");
        case ConversionError::ToolMissing:       return QStringLiteral("缺少转换工具");
        case ConversionError::SourceNotFound:    return QStringLiteral("源文件不存在");
        case ConversionError::UnsupportedFormat: return QStringLiteral("不支持的格式");
        case ConversionError::CorruptFile:       return QStringLiteral("文件损坏");
        case ConversionError::PasswordProtected: return QStringLiteral("PDF 已加密，请提供密码或跳过");
        case ConversionError::ScannedPdfNoOcr:   return QStringLiteral("此为扫描件，首期不支持 OCR，请用 OCR 后的版本");
        case ConversionError::Timeout:           return QStringLiteral("转换超时");
        case ConversionError::Cancelled:         return QStringLiteral("已取消");
        case ConversionError::Unknown:           return QStringLiteral("未知错误");
    }
    return QStringLiteral("未知错误");
}

Format formatFromString(const QString& s) {
    QString lower = s.toLower().trimmed();
    if (lower == QLatin1String("markdown") || lower == QLatin1String("md"))
        return Format::Markdown;
    if (lower == QLatin1String("docx")) return Format::DOCX;
    if (lower == QLatin1String("html") || lower == QLatin1String("htm"))
        return Format::HTML;
    if (lower == QLatin1String("pdf"))  return Format::PDF;
    return Format::Unknown;
}

QString formatToString(Format f) {
    switch (f) {
        case Format::Markdown: return QStringLiteral("markdown");
        case Format::DOCX:     return QStringLiteral("docx");
        case Format::HTML:     return QStringLiteral("html");
        case Format::PDF:      return QStringLiteral("pdf");
        case Format::Unknown:  return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

ConversionDirection directionFromFormats(Format s, Format t) {
    if (s == Format::Markdown && t == Format::DOCX) return ConversionDirection::MD_to_DOCX;
    if (s == Format::DOCX && t == Format::Markdown) return ConversionDirection::DOCX_to_MD;
    if (s == Format::Markdown && t == Format::HTML) return ConversionDirection::MD_to_HTML;
    if (s == Format::HTML && t == Format::Markdown) return ConversionDirection::HTML_to_MD;
    if (s == Format::Markdown && t == Format::PDF)  return ConversionDirection::MD_to_PDF;
    if (s == Format::PDF && t == Format::Markdown)  return ConversionDirection::PDF_to_MD;
    return ConversionDirection::Unknown;
}

} // namespace conversion
} // namespace dmc
