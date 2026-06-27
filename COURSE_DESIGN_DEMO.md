# DocMind AI 课程设计版说明

这是一个简化整合版 Demo，目标是覆盖课程设计展示所需的三个核心能力：

1. Markdown 文本编辑器
   - 新建、打开、编辑 Markdown / 文本文档
   - 实时 Markdown 预览
   - 保存为 Markdown
   - 导出为 HTML

2. 文档导入与知识库
   - 从文件导入 Markdown / DOCX / PDF / HTML 到知识库
   - 将当前编辑器内容直接加入知识库
   - 支持清空知识库
   - 没有 API Key 时会退化为关键词检索，不影响导入和演示

3. AI 功能
   - 保存 OpenAI-compatible API 设置
   - 文本生成 Base URL 与向量 Base URL 可分开配置
   - AI 润色选中内容或全文
   - AI 摘要全文
   - 知识库问答，回答附带来源
   - 输入 `general: 你的问题` 可直接与 AI 对话

## 本机构建方式

本项目当前使用仓库内隔离依赖，不修改系统环境：

```bash
arch -x86_64 .deps/venv/bin/cmake -S . -B .build/course-design-demo-qt683-x64-sdk154 \
  -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$PWD/.deps/venv/bin/ninja" \
  -DCMAKE_PREFIX_PATH="$PWD/.deps/Qt/6.8.3/macos" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk

arch -x86_64 .deps/venv/bin/cmake --build .build/course-design-demo-qt683-x64-sdk154 --parallel
```

可执行文件：

```bash
.build/course-design-demo-qt683-x64-sdk154/DocMindAI
```

## 演示建议

1. 打开程序后，在左侧编辑器输入一段 Markdown。
2. 点击“导出 HTML”，展示导出能力。
3. 点击“加入知识库”，再在右侧问答框提问。
4. 如果配置了 API Key，可以展示 AI 润色、AI 摘要和知识库问答。
5. 如果没有 API Key，可以展示编辑器、导入导出、关键词检索和 API Key 未配置提示。

## 阿里百炼示例配置

如果文本生成和向量都使用阿里百炼兼容 OpenAI 接口，可以这样填：

- 文本 Base URL：`https://dashscope.aliyuncs.com/compatible-mode`
- 向量 Base URL：`https://dashscope.aliyuncs.com/compatible-mode`
- 聊天模型：`qwen-plus`
- 嵌入模型：`text-embedding-v4`

如果文本生成使用别的 OpenAI-compatible 服务，而向量使用阿里百炼，只需要把“向量 Base URL”单独填成阿里地址即可。
