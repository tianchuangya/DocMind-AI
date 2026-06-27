# DocMind-AI

## Module B: Conversion Service

异步、可取消的文档转换服务，支持多种格式转换和文本提取。

### 功能特性

- **格式转换**
  - Markdown ↔ DOCX (Pandoc)
  - Markdown ↔ HTML (Pandoc)
  - Markdown → PDF (Tectonic)
  - PDF → Markdown (Poppler pdftotext)

- **文本提取 (extractText)**
  - 内存模式，不产生中间文件
  - 返回结构化文本块 (StructBlock)
  - 支持页码追踪 (SourceSpan)
  - 自动检测扫描PDF和加密PDF

- **任务管理**
  - 异步任务队列
  - 任务取消支持
  - 批量转换
  - 进度回调

- **错误处理**
  - `ScannedPdfNoOcr` - 扫描PDF需OCR
  - `PasswordProtected` - 加密PDF
  - `ToolMissing` - 工具缺失
  - 临时文件自动清理

### 依赖工具

| 工具 | 用途 | 安装 |
|------|------|------|
| Pandoc | MD↔DOCX, MD↔HTML | `brew install pandoc` |
| Tectonic | MD→PDF | `brew install tectonic` |
| Poppler | PDF→MD | `brew install poppler` |

### 构建

```bash
mkdir build && cd build
cmake ..
make
```

或使用 g++ 直接编译：

```bash
c++ -std=c++20 -pthread -o ConversionService src/conversion/ConversionService.cpp
```

### 使用

```bash
./ConversionService
```

交互式菜单：
1. Convert - 格式转换
2. Extract - 文本提取
3. Tools - 工具状态
4. Cancel - 取消任务
5. Stats - 统计信息
6. Caps - 能力探测
7. Help - 帮助
0. Exit - 退出

### 接口说明

```cpp
namespace ConversionService {
    struct TextExtractionRequest {
        std::string source_path;
        std::string source_format;
        bool prefer_structure{true};
    };
    
    struct TextExtractionResult {
        std::string plain_text;
        std::string markdown_text;
        std::vector<StructBlock> blocks;
        std::vector<SourceSpan> spans;
        ConversionError error_code;
        bool ok;
    };
    
    class ConversionEngine {
        TaskHandle convert(const TaskInput& in);
        TextExtractionResult extractText(const TextExtractionRequest& req);
        bool cancel(TaskHandle h);
        ConversionCapabilities capabilities() const;
    };
}
```

### C++ 标准

支持 C++17 和 C++20。
