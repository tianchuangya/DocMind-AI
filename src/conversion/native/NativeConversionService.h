#pragma once

#include "../Types.h"
#include <QObject>
#include <QHash>
#include <QTimer>
#include <memory>

namespace dmc {
namespace conversion {

class NativeMarkdownConverter;
class NativeDocxConverter;
class NativePdfConverter;

// 原生转换服务 — 不依赖外部工具的统一接口
class NativeConversionService : public QObject {
    Q_OBJECT
public:
    explicit NativeConversionService(QObject* parent = nullptr);
    ~NativeConversionService() override;

    TaskHandle submitConversion(const TaskInput& input);
    TaskOutput getTaskStatus(TaskHandle handle) const;
    bool cancelTask(TaskHandle handle);
    bool canConvert(Format source, Format target) const;
    QVector<ConversionDirection> supportedDirections() const;
    ServiceStats stats() const;

signals:
    void taskSubmitted(TaskHandle handle);
    void taskStarted(TaskHandle handle);
    void taskCompleted(TaskHandle handle, const TaskOutput& output);
    void taskFailed(TaskHandle handle, const TaskOutput& output);

private:
    std::unique_ptr<NativeMarkdownConverter> m_markdown_converter;
    std::unique_ptr<NativeDocxConverter>     m_docx_converter;
    std::unique_ptr<NativePdfConverter>      m_pdf_converter;

    QHash<TaskHandle, TaskOutput> m_tasks;
    TaskHandle m_next_handle{1};

    size_t m_total_tasks{0};
    size_t m_completed_tasks{0};
    size_t m_failed_tasks{0};

    TaskOutput executeConversion(const TaskInput& input);
};

} // namespace conversion
} // namespace dmc
