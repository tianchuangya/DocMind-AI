# DocMindAI 智能文本编辑器

DocMindAI 是一个基于 **C++20 + Qt 6** 的课程设计项目，目标是做出一个可演示、可编译、功能闭环完整的智能文本编辑器。项目当前不是单独的“模块 B 文档转换服务”，而是已经把模块 A、模块 B、模块 C 合并到主分支的综合版本。

## 当前实现状态

当前 `main` 分支实现的是课程设计可交付版本，核心功能包括：

- Markdown 编辑器界面：新建、打开、编辑、实时预览、保存 Markdown。
- 导入与导出：支持导出 HTML、DOCX、PDF；支持导入 Markdown、DOCX、PDF、HTML 到知识库。
- 文档转换中枢：模块 B 提供统一转换入口，优先调用 Pandoc / Tectonic / Poppler，缺少外部工具时使用项目内置的原生 DOCX / PDF / HTML 兜底转换。
- AI 写作功能：支持 OpenAI-compatible 接口配置，可进行文本润色、全文摘要。
- 知识库问答：导入资料后写入本地 SQLite，支持关键词检索、向量检索和带来源的问答。
- 本地数据管理：设置、知识库条目、分块、全文索引和向量信息均由模块 C 管理。

## 模块划分

### 模块 A：编辑与预览核心

负责用户直接看到和操作的文本编辑体验，主要代码位于：

- `src/app/`
- `src/core/`
- `src/editor/`
- `src/preview/`
- `src/widgets/`

已覆盖课程设计演示所需的 Markdown 编辑、实时预览、保存、导出入口等能力。

### 模块 B：文档转换中枢

负责导入导出和文本提取，主要代码位于：

- `src/conversion/`
- `src/conversion/native/`

当前支持：

| 能力 | 实现方式 |
| --- | --- |
| Markdown 导出 HTML | 内置转换 / 转换服务 |
| Markdown 导出 DOCX | 优先 Pandoc，缺失时使用原生 DOCX 转换器 |
| Markdown 导出 PDF | 优先 Pandoc + Tectonic，缺失时使用原生 PDF 转换器 |
| DOCX / PDF / HTML 导入知识库 | 先提取文本，再交给模块 C 入库 |

### 模块 C：AI 服务与数据管理层

负责 AI Provider、知识库、SQLite 存储和设置管理，主要代码位于：

- `src/ai/`
- `src/knowledge/`
- `src/storage/`
- `src/sync/`

当前支持：

- 文本生成 Base URL 与向量 Base URL 分开配置。
- 支持阿里百炼等 OpenAI-compatible 服务。
- 支持没有 API Key 时退化为本地关键词检索，保证课程设计演示不中断。

## 构建方式

本项目推荐使用仓库内隔离依赖构建，不需要修改系统 Qt、CMake 或 Python 环境。

在本机当前环境中可使用：

```bash
arch -x86_64 .deps/venv/bin/cmake -S . -B .build/main-export-menu \
  -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$PWD/.deps/venv/bin/ninja" \
  -DCMAKE_PREFIX_PATH="$PWD/.deps/Qt/6.8.3/macos" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk

arch -x86_64 .deps/venv/bin/cmake --build .build/main-export-menu --parallel
```

构建成功后的可执行文件：

```bash
.build/main-export-menu/DocMindAI
```

如果在其他机器上构建，需要先准备 Qt 6、CMake 和 Ninja，然后把 `CMAKE_PREFIX_PATH` 改成对应机器的 Qt 安装路径。

## 使用方式

1. 启动 `DocMindAI`。
2. 在左侧编辑器输入或打开 Markdown 文档。
3. 使用预览区域查看渲染效果。
4. 通过导出菜单选择：
   - 导出 HTML
   - 导出 DOCX
   - 导出 PDF
5. 点击“加入知识库”或导入外部文档，将内容写入本地知识库。
6. 在问答区域输入问题，查看知识库检索结果和 AI 回答。
7. 如需 AI 润色、摘要或向量检索，在设置中填写兼容 OpenAI 的 API 信息。

## 阿里百炼配置示例

如果文本生成和向量模型都使用阿里百炼兼容 OpenAI 接口，可以这样配置：

- 文本 Base URL：`https://dashscope.aliyuncs.com/compatible-mode`
- 向量 Base URL：`https://dashscope.aliyuncs.com/compatible-mode`
- 聊天模型：`qwen-plus`
- 嵌入模型：`text-embedding-v4`

如果聊天模型和向量模型来自不同服务，只需要分别填写文本生成 Base URL 和向量 Base URL。

## 文档说明

- `PLAN.md`：完整项目规划，偏“宏伟版”设计。
- `COURSE_DESIGN_DEMO.md`：课程设计演示版说明。
- `docs/`：模块对齐、接口讨论和历史评审资料。
- `docs/archive/`：开发过程中产生的临时 Markdown 测试文件，保留用于追溯，不作为项目正式说明。

## 项目定位

当前版本定位为课程设计综合成品：功能不追求商业级完整度，但已经具备“编辑器界面 + 导入导出 + AI 功能 + 本地知识库”的完整闭环。
