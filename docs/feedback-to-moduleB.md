# 模块 B 代码评审与必须修复的问题

> 归档说明：本文是早期 `feat/initial-module-b` 分支的代码评审记录，用于保留开发过程和课程设计工作日志依据。当前 `main` 分支已经完成模块 A/B/C 整合，模块 B 不再是“只有一个 cpp”的状态；项目正式说明请以根目录 `README.md` 为准。

- **评审对象**:`feat/initial-module-b` 分支,`src/conversion/ConversionService.cpp`
- **评审人**:模块 C 负责人
- **结论**:**字段定义和数据流方向对了,但实现无法集成,必须返工**。以下 6 项是阻塞性问题,不改模块 C 无法使用你的代码。每项都给了**具体改法**,照着做即可。

---

## ⛔ 阻塞性问题(必须改)

### 问题 1:没有头文件

你只上传了 `ConversionService.cpp`(817 行,还带 `main()`),**没有 `ConversionService.h`**。

模块 C 要 `#include` 你的接口声明才能调用,现在没有任何东西可以 include。

**改法**:把所有 `enum / struct / class` 的**声明**移到头文件,**实现**留在 `.cpp`。

```
src/conversion/
├── ConversionService.h     ← 新建,放所有声明
└── ConversionService.cpp   ← 只留实现
```

`main()` 那个 demo 删掉,或者单独拆到 `examples/` 目录,不要和库代码混在一起。

---

### 问题 2:命名空间错了

你用的是:
```cpp
namespace ConversionService { ... }   // ❌ 大写,不符合项目约定
```

整个项目用的是 `namespace dmc`(模块 A 全程如此,见 `src/core/DocumentSession.h`、`src/utils/Logger.h`)。

**改法**:
```cpp
namespace dmc {
namespace conversion {   // 或者直接全放 dmc 下
    // ... 你的所有代码
}
}
```

CMake 里 target 已经设了 include 路径 `src/`,头文件引用用 `#include "conversion/ConversionService.h"`。

---

### 问题 3:不是 Qt 类,extractText 是同步阻塞的 ⚠️ 最严重

你的 `ConversionEngine` 是个**纯 C++ 类**,`extractText` **直接同步调用 `run_proc` 等子进程返回**。

```cpp
TextExtractionResult extractText(const TextExtractionRequest& req) {
    // 这里直接调 pandoc/pdftotext,会阻塞调用线程几十秒!
    ...
}
```

**后果**:模块 C 在 UI 线程调它,界面会**完全卡死**到 PDF 提取完成(可能几十秒)。PLAN 明确要求"异步、可取消"。

**改法**:让它继承 `QObject`,用 `QtConcurrent` 把同步实现包成异步,通过信号通知完成。

```cpp
// ConversionService.h
#include <QObject>
#include <QFuture>

namespace dmc::conversion {

class ConversionEngine : public QObject {
    Q_OBJECT
public:
    explicit ConversionEngine(QObject* parent = nullptr);

    // 异步版本:立即返回,完成后发信号
    void extractTextAsync(const TextExtractionRequest& req);
    // 同步版本:保留(给测试用)
    TextExtractionResult extractText(const TextExtractionRequest& req);

signals:
    void extractionFinished(const TextExtractionRequest& req,
                             const TextExtractionResult& result);
    void extractionFailed(const TextExtractionRequest& req,
                          ConversionError code,
                          const QString& message);
};

} // namespace dmc::conversion
```

```cpp
// ConversionService.cpp
#include <QtConcurrent/QtConcurrent>

void ConversionEngine::extractTextAsync(const TextExtractionRequest& req) {
    // this 指针作为 parent,保证 lambda 里调用成员安全
    QtConcurrent::run([this, req]() {
        TextExtractionResult result = this->extractText(req);  // 复用你现有的同步实现
        if (result.ok) {
            emit extractionFinished(req, result);
        } else {
            emit extractionFailed(req, result.error_code, QString::fromStdString(result.error));
        }
    });
}
```

你现有的同步 `extractText` 实现可以**原封不动保留**,只是包一层异步。

**取消**:首期可以不做细粒度取消,但至少要支持"批次取消"(用户取消整个导入任务,不再发新的 `extractTextAsync`)。

---

### 问题 4:TextExtractionRequest 不支持内存源

你的 `TextExtractionRequest` 只有 `source_path`:
```cpp
struct TextExtractionRequest {
    std::string source_path;
    std::string source_format;
    bool prefer_structure{true};
};
```

但 PLAN 写"任务输入为源文件/**当前文档快照**"。模块 C 的场景:用户在编辑器里写了个 Markdown **没保存到磁盘**,想直接入库到知识库 —— 这时没有文件路径,只有内存内容。

模块 A 已经提供了 `DocumentSnapshot`(在 `src/core/DocumentSession.h`),里面有 `content` 字段。模块 C 会拿 snapshot 给你,你要支持从内存内容提取。

**改法**:加一个内存源字段,优先级高于 path。

```cpp
struct TextExtractionRequest {
    std::string source_path;        // 磁盘文件路径(二选一)
    std::string source_content;     // 内存内容(优先于 path,非空时用这个)
    std::string source_format;      // md/docx/pdf/html
    bool prefer_structure{true};
};
```

`extractText` 里判断:
```cpp
if (!req.source_content.empty()) {
    // 从内存提取,不读盘
    std::string content = req.source_content;
    // ... 直接 parse_blocks 等
} else {
    // 走现有的文件路径分支
}
```

对 Markdown 内存源来说尤其简单 —— 直接 `parse_blocks(content)` 就行,不涉及外部工具。

---

### 问题 5:没有更新 CMakeLists.txt

你上传了 `.cpp`,但**没改 CMakeLists.txt**。模块 A 的 CMakeLists 里 `APP_SOURCES` 列表没有你的文件。

**后果**:合并到 main 后,你的 `ConversionService.cpp` **不会被编译**,等于白上传。

**改法**:在 CMakeLists.txt 的 `APP_SOURCES` 和 `APP_HEADERS` 里追加:

```cmake
set(APP_SOURCES
    # ... 现有的 ...
    src/conversion/ConversionService.cpp
)

set(APP_HEADERS
    # ... 现有的 ...
    src/conversion/ConversionService.h
)
```

另外 CMake 现在 **缺 `Qt6::Sql` 和 `Qt6::Concurrent`**:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network PrintSupport Sql Concurrent   # ← 加 Sql 和 Concurrent
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::PrintSupport
    Qt6::Sql Qt6::Concurrent                                   # ← 加这两个
)
```

- `Qt6::Concurrent`:问题 3 的 `QtConcurrent::run` 需要。
- `Qt6::Sql`:模块 C 的 SQLite 依赖(模块 A 已经被要求加,你这边一起加上避免合并冲突)。

---

### 问题 6:批量导入没有逐文件进度回调

你的 `convert_batch`:
```cpp
std::vector<TaskHandle> convert_batch(const std::vector<TaskInput>& inputs) {
    // 回调里只能 done_++,拿不到"是哪个文件完成了 + 结果是什么"
}
```

模块 C 的批量入库流程:用户选 50 个 PDF → 入库 → UI 进度条要显示"已完成 12/50,正在处理 doc_13.pdf"。

现在你的回调**只传 `TaskOutput`,不传 `TaskInput`**,模块 C 知道完成了但不知道是哪个文件、文本内容是什么。

**改法**:回调签名带上输入或源标识:

```cpp
using TaskCallback = std::function<void(const TaskInput& input,
                                         const TaskOutput& output)>;

TaskHandle enqueue(const TaskInput& in, TaskCallback cb = nullptr);
```

或者更简单 —— 模块 C 入库走的是 `extractTextAsync` 而不是 `convert`,所以只要问题 3 的信号 `extractionFinished(req, result)` 里 `req` 完整带回去就行(模块 C 自己维护"哪个 req 对应哪个文件")。**这条通过问题 3 的修复即可间接满足**,只要你保证 `extractionFinished` 把原始 `req` 完整回传。

---

## ⚠️ 次要问题(不阻塞,但强烈建议改)

### 问题 7:用 std::string 而不是 QString

你全程用 `std::string`。模块 A 全程用 `QString`(Qt 的字符串,自动处理 Unicode)。

集成时模块 C 要在两边来回转换,容易出编码 bug(尤其是中文路径、中文内容)。

**建议**:所有对外接口的字符串字段改成 `QString`,内部实现需要 `std::string` 时再 `.toStdString()` 转一下。

### 问题 8:is_scanned_pdf 启发式过于粗暴

```cpp
bool is_scanned_pdf(...) const {
    // 单词数 < 50 就判定为扫描件
    return wc < 50;
}
```

正常的短 PDF(比如一封简短的 PDF 信件)会被误判为扫描件,无法入库。

**建议**:
- 阈值改成基于"图片覆盖率"(扫描件几乎全是图片,几乎没有文本层)。
- 或者用 `pdfimages` 统计图片数量 + `pdftotext` 文本量综合判断。
- 至少把阈值降到 `< 10` 个单词,并加日志方便排查。

### 问题 9:PDF 提取用 -layout 模式会保留大量空格

```cpp
RunResult pdf_to_md(...) {
    return run_proc(p, {"-layout", in, out});
}
```

`-layout` 模式会保留 PDF 的视觉版式空格,导致文本里到处是多空格、对齐用的空格。对 RAG 分块不友好 —— 分块器可能把空格当成词的一部分。

**建议**:提供 `-raw` 模式选项,或加一个 `TextExtractionRequest::layout_mode` 字段让调用方选。模块 C 默认用 `-raw`。

---

## 验收清单(改完请逐项自测)

- [ ] 拆出 `ConversionService.h`,纯声明无实现
- [ ] 命名空间改为 `dmc::conversion`
- [ ] `ConversionEngine` 继承 `QObject`,加 `Q_OBJECT` 宏
- [ ] 提供 `extractTextAsync` + `extractionFinished` 信号
- [ ] `TextExtractionRequest` 加 `source_content` 字段(内存源)
- [ ] CMakeLists.txt 加入新文件 + `Qt6::Sql` + `Qt6::Concurrent`
- [ ] 批量回调能拿到"哪个输入完成了"(通过信号回传 req 即可)
- [ ] `extractTextAsync` 完成后不卡 UI 线程(在测试程序里调一个 10MB PDF,UI 要保持响应)
- [ ] 临时文件在异步路径下也能正确清理(`cleanup()` 在 worker 线程执行,不要在 UI 线程碰文件)

---

## 给你的集成示例(改完后模块 C 会这么用)

```cpp
#include "conversion/ConversionService.h"

void KnowledgeIngestionService::ingest(const QString& filePath) {
    using namespace dmc::conversion;

    TextExtractionRequest req;
    req.source_path = filePath.toStdString();
    req.source_format = "pdf";
    req.prefer_structure = true;

    // 异步,不阻塞 UI
    m_engine->extractTextAsync(req);

    // 连接一次信号
    connect(m_engine, &ConversionEngine::extractionFinished,
            this, [this](const TextExtractionRequest& req, const TextExtractionResult& result) {
        // 拿到 blocks + spans + 页码,开始分块和嵌入
        chunkAndEmbed(result.blocks, result.spans);
        emit fileIngested(QString::fromStdString(req.source_path));
    });

    connect(m_engine, &ConversionEngine::extractionFailed,
            this, [this](const TextExtractionRequest& req, ConversionError code, const QString& msg) {
        emit fileFailed(QString::fromStdString(req.source_path), code, msg);
    });
}
```

照这个用法,你就能反推接口需要长什么样。

---

## 总结

你的**数据结构设计**(StructBlock / SourceSpan / 错误枚举 / PDF 分页)**是对的**,和模块 C 的诉求对得上。

但**工程封装**完全没做:没头文件、命名空间错、不继承 QObject、同步阻塞、没进 CMake、不支持内存源。这 6 项不解决,你的代码进不了主分支。

改完发个 PR 或推到 `feat/initial-module-b`,模块 C 这边会重新核对。
