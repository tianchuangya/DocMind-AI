#include "NativeConversionService.h"
#include "NativeMarkdownConverter.h"
#include "NativeDocxConverter.h"
#include "NativePdfConverter.h"
#include <QTimer>

namespace dmc {
namespace conversion {

NativeConversionService::NativeConversionService(QObject* parent) : QObject(parent) {
    m_markdown_converter = std::make_unique<NativeMarkdownConverter>();
    m_docx_converter     = std::make_unique<NativeDocxConverter>();
    m_pdf_converter      = std::make_unique<NativePdfConverter>();
}

NativeConversionService::~NativeConversionService() {}

TaskHandle NativeConversionService::submitConversion(const TaskInput& input) {
    m_total_tasks++;
    TaskHandle handle = m_next_handle++;

    TaskOutput output;
    output.status = TaskStatus::Running;
    m_tasks[handle] = output;

    emit taskSubmitted(handle);
    emit taskStarted(handle);

    QTimer::singleShot(0, this, [this, handle, input]() {
        TaskOutput result = executeConversion(input);
        m_tasks[handle] = result;

        if (result.status == TaskStatus::Completed) {
            m_completed_tasks++;
            emit taskCompleted(handle, result);
        } else {
            m_failed_tasks++;
            emit taskFailed(handle, result);
        }
    });

    return handle;
}

TaskOutput NativeConversionService::getTaskStatus(TaskHandle handle) const {
    if (m_tasks.contains(handle))
        return m_tasks[handle];
    TaskOutput o;
    o.status = TaskStatus::Failed;
    o.error_message = QStringLiteral("任务不存在");
    return o;
}

bool NativeConversionService::cancelTask(TaskHandle handle) {
    if (m_tasks.contains(handle)) {
        m_tasks[handle].status = TaskStatus::Cancelled;
        return true;
    }
    return false;
}

bool NativeConversionService::canConvert(Format source, Format target) const {
    if (source == Format::Markdown && target == Format::HTML) return true;
    if (source == Format::HTML && target == Format::Markdown) return true;
    if (source == Format::Markdown && target == Format::DOCX) return true;
    if (source == Format::DOCX && target == Format::Markdown) return true;
    if (source == Format::Markdown && target == Format::PDF)  return true;
    if (source == Format::PDF && target == Format::Markdown)  return true;
    return false;
}

QVector<ConversionDirection> NativeConversionService::supportedDirections() const {
    return {
        ConversionDirection::MD_to_HTML, ConversionDirection::HTML_to_MD,
        ConversionDirection::MD_to_DOCX, ConversionDirection::DOCX_to_MD,
        ConversionDirection::MD_to_PDF,  ConversionDirection::PDF_to_MD,
    };
}

ServiceStats NativeConversionService::stats() const {
    ServiceStats s;
    s.total_tasks     = m_total_tasks;
    s.completed_tasks = m_completed_tasks;
    s.failed_tasks    = m_failed_tasks;
    return s;
}

TaskOutput NativeConversionService::executeConversion(const TaskInput& input) {
    auto dir = directionFromFormats(input.source_format, input.target_format);
    switch (dir) {
        case ConversionDirection::MD_to_HTML:
            return m_markdown_converter->exportToHtml(input);
        case ConversionDirection::HTML_to_MD:
            return m_markdown_converter->importFromHtml(input);
        case ConversionDirection::MD_to_DOCX:
            return m_docx_converter->exportToDocx(input);
        case ConversionDirection::DOCX_to_MD:
            return m_docx_converter->importFromDocx(input);
        case ConversionDirection::MD_to_PDF:
            return m_pdf_converter->exportToPdf(input);
        case ConversionDirection::PDF_to_MD:
            return m_pdf_converter->importFromPdf(input);
        default: {
            TaskOutput o;
            o.status       = TaskStatus::Failed;
            o.error_code   = ConversionError::UnsupportedFormat;
            o.error_message = QStringLiteral("不支持的转换方向");
            return o;
        }
    }
}

} // namespace conversion
} // namespace dmc
