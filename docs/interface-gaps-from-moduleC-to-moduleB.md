# 模块 C → 模块 B 接口对齐清单

- **来源**:模块 C(AI 服务与知识库)负责人
- **目标**:模块 B(文档转换中枢)负责人
- **目的**:模块 C 的知识库需要导入 Markdown / DOCX / PDF / HTML 并提取纯文本与结构,而模块 B 的 `ConversionService` 已覆盖这些格式的转换。模块 C **不重复造轮子**,直接复用模块 B 的能力。以下是需要模块 B 提供/确认的接口与边界。

---

## 一、核心诉求:知识库入库需要"文本提取"入口

### 1. 提供一个"提取纯文本 + 基础结构"的入口(不是文件到文件)

**现状(按 PLAN/接口.md)**:模块 B 的 `ConversionService` 输入是"源文件/快照 + 目标格式 + 输出路径",产物是**文件**(如 DOCX→MD 写到磁盘)。

**模块 C 的问题**:知识库入库需要的是**内存中的文本 + 结构信息**,不需要中间落盘文件(否则要自己再读回来 + 清理临时文件,浪费 IO 且容易泄漏)。

**需要模块 B 提供**:
```cpp
// 伪代码,字段请模块 B 定稿
struct TextExtractionRequest {
    QString sourcePath;            // 源文件路径
    QString sourceFormat;          // md/docx/pdf/html,可省略由 B 探测
    bool    preferStructure = true;// 是否尽量保留标题/段落结构
};

struct TextExtractionResult {
    QString plainText;            // 提取后的纯文本(已合并为可分块的流)
    QString markdownText;         // 若可还原为 MD 结构(标题/列表/代码块),优先给这个
    QList<StructBlock> blocks;    // 结构化块(见 #2)
    QList<SourceSpan>  spans;     // 来源定位(见 #3)
    QString error;
    bool   ok = false;
};

class ConversionService {
    // 已有的文件→文件转换 ...
    // 新增:面向知识库的文本提取(异步,可取消)
    QFuture<TextExtractionResult> extractText(const TextExtractionRequest& req);
};
```

**关键点**:
- 返回 **MD 结构文本** 优于纯文本 —— 模块 C 可按 `#` 标题分块,质量远高于按字数硬切。
- 若 B 当前只有"文件→文件"的 ConversionJob,**请增加一个"内存结果"模式**(`ConversionRequest::outputPath` 留空时返回内存文本,而非写盘)。

### 2. 结构化块(StructBlock)—— 供模块 C 分块用

知识库分块策略:按标题 + 长度切分。需要 B 在提取时给出结构标记。

**需要字段(建议)**:
```cpp
struct StructBlock {
    enum Type { Heading, Paragraph, ListItem, CodeBlock, TableCell, Blockquote };
    Type    type;
    int     level = 0;          // 标题级别(1-6),仅 Heading 有意义
    QString text;               // 该块文本(已去格式标记)
    int     sourceLine  = -1;   // 源文档内行号/位置(若可获取)
    int     sourcePage  = -1;   // PDF 页码(见 #3)
};
```

**至少需要**:能区分**标题**与**正文**(这样模块 C 可以"标题之下到下一标题之前"作为一块)。代码块/列表/表格能区分更好,但标题是底线。

### 3. 来源定位(SourceSpan)—— RAG 引用必须

PLAN 要求知识库问答"展示可点击的来源引用"。模块 C 必须知道命中文本来自源文档的**哪一页/哪一段**,否则引用无法回链。

**需要模块 B 在提取 PDF 时保留页码**(Poppler 支持,但需 B 主动暴露):
```cpp
struct SourceSpan {
    int     page     = -1;   // PDF 页码;DOCX/MD/HTML 可为 -1
    int     lineStart = -1;  // 源内行号
    int     charStart = -1;  // 字符偏移(备选)
    QString anchor;          // HTML 锚点 / MD 标题 slug(若可生成)
};
```

**关键问题**:B 当前 PDF 提取是否保留页码边界?若没有,**请增加**:每段文本附带 `page`。这是模块 C 能否做"点击引用跳到 PDF 第 N 页"的前提。

### 4. DOCX 标题级别保留

DOCX → MD 转换时,Pandoc 默认会保留 Heading 1-6。模块 C 依赖此做分块。

**确认**:B 的 DOCX 导入是否保留标题层级?(Pandoc 默认保留,但若 B 做了扁平化处理需告知。)

---

## 二、异步、取消、进度

### 5. 提取过程的异步与取消

知识库批量导入(几十个 PDF)时,用户可能取消。需要 B 的 `extractText` 支持:

- **异步**:`QFuture` 或信号槽(模块 C 不阻塞 UI 线程)。
- **取消**:传入 `QFuture` 可取消,或提供 `cancel(jobId)`。
- **进度**:批量场景下,逐文件完成信号(`fileExtracted` / `batchProgress`)。单文件内的进度可不要(Poppler 很快)。

**需要模块 B 明确**:
- `extractText` 返回 `QFuture` 还是发信号?
- 取消语义:取消后已提取的部分是否丢弃?(模块 C 期望:丢弃,不入库)

### 6. 失败错误映射

模块 C 入库界面要把 B 的失败翻译成用户可读的提示。需要 B 的错误包含:

```cpp
enum class ConversionError {
    None,
    ToolMissing,        // Pandoc/Poppler 未捆绑或不可执行
    SourceNotFound,
    UnsupportedFormat,
    CorruptFile,        // PDF 损坏、DOCX 解析失败
    PasswordProtected,  // PDF 加密(Poppler 常见)
    ScannedPdfNoOcr,    // 扫描件无文本层(PLAN 已说明首期不做 OCR)
    Timeout,
    Cancelled,
    Unknown,
};
```

**特别需要**:`ScannedPdfNoOcr` 与 `PasswordProtected` 必须可区分 —— 模块 C 要给用户不同的提示("此 PDF 为扫描件,首期不支持" vs "此 PDF 已加密,请提供密码或跳过")。

---

## 三、工具探测与降级

### 7. 转换能力探测接口

PLAN 提到模块 B 有"转换能力探测页"。模块 C 在知识库导入入口需要据此**禁用不可用的格式**(如 Poppler 缺失时,PDF 导入按钮置灰)。

**需要模块 B 暴露**:
```cpp
struct ConversionCapabilities {
    bool pandocOk    = false;
    bool tectonicOk  = false;
    bool popplerOk   = false;
    QString pandocVersion;
    // ...
};
ConversionCapabilities ConversionService::capabilities() const;
```

或一个简单的 `bool canExtract(Format f)`。模块 C 不需要知道工具路径,只要"能不能用"。

### 8. 临时文件清理

B 在转换过程中会创建临时目录(解压 DOCX、Pandoc 中间产物等)。

**需要明确**:
- `extractText` 完成后,B 是否自动清理临时文件?(模块 C 期望:是,内存模式不应残留磁盘产物)
- 若 B 写了中间文件,**请由 B 负责清理**,不要让 C 去删 B 的临时文件。

---

## 四、资源与编码

### 9. 图片资源处理

知识库入库**不需要图片**(只索文本),但需要确认:
- B 提取文本时是否会被"图片资源缺失"阻断?(例如 DOCX 引用外部图片)
- 模块 C 期望:**忽略图片,只取文本**,图片缺失不应导致提取失败。

### 10. 编码

- PDF:Poppler 直接给 Unicode,无编码问题。
- DOCX:Pandoc 处理,无编码问题。
- HTML:可能遇到 GBK 等。**请 B 保证** `extractText` 返回的 `QString` 已正确解码(模块 A 的 DocumentSession 已有 BOM 检测逻辑可参考)。

---

## 五、接口契约一致性(对齐接口.md)

### 11. `ConversionRequest` / `ConversionJob` / `ConversionResult` 的最终签名

接口.md 已列出这三个类型,但模块 C 需要**最终的字段定义**才能调用。请模块 B 提供头文件后告知,或确认以下用例是否被覆盖:

| 模块 C 用例 | 调用方式 | 是否覆盖 |
|---|---|---|
| PDF → 内存文本(含页码) | `extractText(req)` | ⚠️ 需新增内存模式 |
| DOCX → 内存 MD(含标题) | `extractText(req)` | ⚠️ 需新增内存模式 |
| HTML → 内存文本 | `extractText(req)` | ⚠️ 需新增内存模式 |
| MD → 内存文本(直接读) | `extractText(req)` | ✅ 或模块 C 自己读 |
| 批量导入进度 | 信号 | ⚠️ 需明确 |
| 取消单个提取 | `cancel(jobId)` | ⚠️ 需明确 |

---

## 待确认事项汇总(请模块 B 回复)

| # | 事项 | 类型 |
|---|---|---|
| 1 | 是否提供 `extractText` 内存模式(不落盘) | 必需 |
| 2 | `StructBlock` 是否提供(至少标题/正文区分) | 必需 |
| 3 | PDF 提取是否保留页码(`SourceSpan.page`) | 必需(RAG 引用依赖) |
| 4 | DOCX 标题级别是否保留 | 必需 |
| 5 | 异步/取消/进度的具体机制(QFuture vs 信号) | 必需 |
| 6 | 错误枚举是否区分 `ScannedPdfNoOcr` / `PasswordProtected` | 必需 |
| 7 | `ConversionCapabilities` 探测接口 | 必需 |
| 8 | 临时文件清理责任归属(B 负责?) | 必需 |
| 9 | 图片缺失是否阻断文本提取(期望:不阻断) | 确认 |
| 10 | HTML 非 UTF-8 编码是否由 B 解码 | 确认 |
| 11 | 三个类型(Request/Job/Result)最终头文件 | 必需 |

---

## 模块 C 的使用场景(供 B 设计接口时参考)

1. **单文件入库**:用户拖一个 PDF 进知识库 → 调 `extractText` → 拿到 MD + 结构块 + 页码 → 分块 → 嵌入 → 存 SQLite。
2. **批量入库**:用户选 50 个 DOCX → 并发(或排队)`extractText` → 每完成一个发信号更新进度条 → 全部完成后统一嵌入。
3. **问答引用**:用户问"X 在哪定义?" → 向量检索命中块 → 块带 `SourceSpan{page:3}` → UI 显示"来源:doc.pdf 第 3 页"且可点击跳转。
4. **失败提示**:PDF 是扫描件 → B 返回 `ScannedPdfNoOcr` → C 提示"此为扫描件,首期不支持,请用 OCR 后的版本"。
