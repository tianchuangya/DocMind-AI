# DocMindAI — 模块 C：AI 服务与数据管理层

本分支对应 DocMindAI 的 **模块 C**，负责把“智能写作”和“本地知识库”接入到 Qt 桌面文本编辑器中。它不是模块 B 的文档转换服务；模块 B 只在知识库导入 DOCX / PDF / HTML 时作为文本提取能力被模块 C 调用。

## 模块定位

模块 C 的职责是：

- 管理 OpenAI-compatible AI 服务配置；
- 提供文本润色、摘要等写作助手能力；
- 将 Markdown / DOCX / PDF / HTML 文档导入本地知识库；
- 对导入文本进行分块、去重、全文索引和向量化；
- 基于知识库执行关键词检索、向量检索和问答上下文构建；
- 使用 SQLite 保存知识库、设置和非敏感元数据；
- 将 API Key 与普通设置分离，避免密钥直接写入业务表。

## 主要目录

```text
src/ai/          AI Provider、OpenAI-compatible 网络调用、写作助手
src/knowledge/   知识库类型、分块、入库、检索、模块 B 文本提取适配
src/storage/     SQLite 迁移、设置仓库、凭据存储
src/sync/        同步接口占位，当前版本以本地优先为主
src/app/         课程设计演示窗口，负责把模块 A/B/C 串起来
```

## 核心能力

### 1. OpenAI-compatible Provider

相关文件：

- `src/ai/AIProvider.h`
- `src/ai/OpenAICompatibleProvider.h`
- `src/ai/OpenAICompatibleProvider.cpp`

能力说明：

- 支持 Chat Completions；
- 支持 Embeddings；
- 文本生成 Base URL 与向量 Base URL 可分开配置；
- 兼容 OpenAI、阿里百炼等 OpenAI-compatible 服务；
- 使用 `QNetworkAccessManager` 异步请求，避免阻塞 UI；
- 请求失败时会映射为统一错误信息；
- API Key 不写入日志。

### 2. 写作助手

相关文件：

- `src/ai/WritingAssistant.h`
- `src/ai/WritingAssistant.cpp`

当前演示版支持：

- 润色选中内容或全文；
- 摘要全文；
- 将 AI 返回结果回填到界面。

### 3. 知识库入库

相关文件：

- `src/knowledge/KnowledgeIngestionService.h`
- `src/knowledge/KnowledgeIngestionService.cpp`
- `src/knowledge/ChunkingStrategy.h`
- `src/knowledge/ChunkingStrategy.cpp`
- `src/knowledge/ConversionEngineExtractionAdapter.h`

入库流程：

```text
用户选择文件 / 当前编辑器内容
        ↓
模块 B 提取文本（DOCX / PDF / HTML 需要）
        ↓
模块 C 计算 Hash 去重
        ↓
文本分块
        ↓
调用 Embedding 生成向量
        ↓
写入 SQLite 与 FTS5 索引
```

说明：

- Markdown 或当前编辑器内容可以直接入库；
- DOCX / PDF / HTML 先通过模块 B 提取文本，再由模块 C 分块和入库；
- 没有 API Key 时，入库仍可进行，只是向量能力会降级；
- 批量导入通过信号反馈进度。

### 4. 知识库检索与问答

相关文件：

- `src/knowledge/KnowledgeQueryService.h`
- `src/knowledge/KnowledgeQueryService.cpp`
- `src/knowledge/KnowledgeRepository.h`
- `src/knowledge/KnowledgeRepository.cpp`

检索流程：

```text
用户问题
  ↓
关键词检索（FTS5）
  ↓
向量检索（Embedding + 余弦相似度）
  ↓
RRF 融合排序
  ↓
构造带来源的上下文
  ↓
调用聊天模型生成回答
```

如果没有可用的向量模型，系统会退化为关键词检索，保证课程设计演示可以继续进行。

### 5. 设置与本地存储

相关文件：

- `src/storage/DbMigrator.h`
- `src/storage/DbMigrator.cpp`
- `src/storage/SettingsRepository.h`
- `src/storage/SettingsRepository.cpp`
- `src/storage/SecureCredentialStore.h`
- `src/storage/SecureCredentialStore.cpp`

存储内容：

- Provider 配置；
- 模型名称；
- 文本 Base URL；
- 向量 Base URL；
- 知识库文档；
- 文本分块；
- FTS5 索引；
- 向量数据；
- 非敏感应用设置。

API Key 不直接保存在普通设置表中，而是通过 `SecureCredentialStore` 管理。

## 与模块 A / B 的关系

| 关联模块 | 模块 C 如何使用 |
| --- | --- |
| 模块 A：编辑与预览 | 获取当前编辑器内容，用于 AI 润色、摘要、加入知识库 |
| 模块 B：文档转换 | 导入 DOCX / PDF / HTML 时调用文本提取能力 |

模块 C 不负责编辑器渲染，也不直接实现复杂文档格式转换；这两个方向分别由模块 A 和模块 B 负责。

## 阿里百炼配置示例

如果使用阿里百炼兼容 OpenAI 接口：

- 文本 Base URL：`https://dashscope.aliyuncs.com/compatible-mode`
- 向量 Base URL：`https://dashscope.aliyuncs.com/compatible-mode`
- 聊天模型：`qwen-plus`
- 嵌入模型：`text-embedding-v4`

如果文本生成和向量模型来自不同服务，可以分别填写两个 Base URL。

## 构建方式

本项目使用 CMake + Qt 6。当前本机隔离构建命令示例：

```bash
arch -x86_64 .deps/venv/bin/cmake -S . -B .build/feat-initial-module-c-qt683-x64-sdk154 \
  -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$PWD/.deps/venv/bin/ninja" \
  -DCMAKE_PREFIX_PATH="$PWD/.deps/Qt/6.8.3/macos" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk

arch -x86_64 .deps/venv/bin/cmake --build .build/feat-initial-module-c-qt683-x64-sdk154 --parallel
```

构建成功后运行：

```bash
.build/feat-initial-module-c-qt683-x64-sdk154/DocMindAI
```

## 课程设计演示重点

答辩时模块 C 可以这样讲：

1. 打开设置，说明 AI Provider 支持 OpenAI-compatible 接口。
2. 展示文本润色或摘要，说明 `WritingAssistant` 调用聊天模型。
3. 导入文档或将当前编辑器内容加入知识库。
4. 说明入库经过“提取文本 → 分块 → 向量化 → SQLite/FTS5 存储”。
5. 在知识库问答框提问，展示检索结果、AI 回答和来源。
6. 说明没有 API Key 时系统会降级为关键词检索，保证基础功能可用。

## 当前边界

- 当前版本优先保证课程设计展示闭环，不追求商业级知识库性能；
- 向量存储采用 SQLite 本地保存，适合中小规模文档；
- 同步模块目前保留接口，首期以本地优先为主；
- OCR、复杂版式还原、大规模向量数据库属于后续扩展方向。
