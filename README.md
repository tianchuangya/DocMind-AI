# DocMind AI

> 基于 C++ / Qt 6 的桌面 Markdown 智能编辑器 — 模块 A 已完成 ✅

## 项目概览

DocMind AI 是一款面向程序员和学习者的桌面 Markdown 编辑器，以 **"不造轮子，只搭积木"** 为原则，
集成成熟开源库完成解析、渲染、高亮、AI 调用等复杂任务，专注于用 C++ 编写高质量胶水代码与产品化打磨。

## 当前状态 — 模块 A 完成

**模块 A：编辑与预览核心** 已全部完成，可直接编译运行。

### ✅ 已实现功能

| 功能分类 | 功能点 | 状态 |
|---------|--------|------|
| 文档管理 | 多标签文档工作区 | ✅ |
| 文档管理 | 新建 / 打开 / 保存 / 另存为 | ✅ |
| 文档管理 | 最近文件记录 | ✅ |
| 文档管理 | 自动保存（可配置间隔） | ✅ |
| 文档管理 | 关闭前确认提示 | ✅ |
| 编辑器 | Markdown 语法高亮（15+ 元素） | ✅ |
| 编辑器 | 行号显示 | ✅ |
| 编辑器 | 撤销 / 重做 | ✅ |
| 编辑器 | 查找 / 替换 / 全替换 | ✅ |
| 编辑器 | 正则查找 / 全字匹配 / 区分大小写 | ✅ |
| 编辑器 | 跳转到行 | ✅ |
| 编辑器 | Tab 缩进 / Shift+Tab 反缩进 | ✅ |
| 编辑器 | 列表自动延续（有序/无序/任务列表） | ✅ |
| 编辑器 | 引用块自动延续 | ✅ |
| 编辑器 | Ctrl+滚轮缩放字体 | ✅ |
| 编辑器 | UTF-8 / UTF-16 编码支持 | ✅ |
| Markdown | 标题 / 粗体 / 斜体 / 删除线 | ✅ |
| Markdown | 链接 / 图片 / 行内代码 | ✅ |
| Markdown | 围栏代码块（带语言标注） | ✅ |
| Markdown | 表格（含对齐） | ✅ |
| Markdown | 任务列表 `[x]` | ✅ |
| Markdown | 引用块 / 水平线 | ✅ |
| Markdown | 脚注 `[^1]` | ✅ |
| Markdown | 自动链接 | ✅ |
| 预览 | Markdown → HTML 实时预览 | ✅ |
| 预览 | 防抖渲染（350ms） | ✅ |
| 预览 | 编辑器 / 预览 / 分栏三种视图 | ✅ |
| 预览 | 滚动同步 | ✅ |
| 主题 | 浅色主题 | ✅ |
| 主题 | 深色主题（Catppuccin Mocha 风格） | ✅ |
| 主题 | 一键切换 | ✅ |
| UI | 菜单栏 + 工具栏 + 状态栏 | ✅ |
| UI | 拖拽打开文件 | ✅ |
| UI | 设置对话框 | ✅ |
| UI | 会话恢复（重启后打开上次文件） | ✅ |

## 项目结构

```
DocMindAI/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目说明
├── PLAN.md                     # 开发计划
├── LICENSE                     # MIT 许可
│
├── src/                        # 源码（31 个文件，6500+ 行）
│   ├── main.cpp                # 程序入口
│   ├── app/                    # 应用层
│   │   ├── Application.h/cpp   # 应用主类
│   │   ├── MainWindow.h/cpp    # 主窗口
│   │   └── SettingsDialog.h/cpp # 设置对话框
│   ├── core/                   # 数据模型
│   │   ├── DocumentSession.h/cpp # 文档会话管理
│   │   └── AppState.h/cpp      # 全局应用状态
│   ├── editor/                 # 编辑器
│   │   ├── MarkdownEditor.h/cpp  # 编辑器核心控件
│   │   ├── MarkdownHighlighter.h/cpp # 语法高亮器
│   │   ├── LineNumberArea.h/cpp  # 行号区域
│   │   └── FindReplaceDialog.h/cpp # 查找替换对话框
│   ├── preview/                # 预览
│   │   ├── MarkdownRenderer.h/cpp # Markdown→HTML 渲染器
│   │   ├── PreviewWidget.h/cpp    # 预览控件
│   │   └── ScrollSyncManager.h/cpp # 滚动同步管理器
│   ├── widgets/                # UI 组件
│   │   ├── TabManager.h/cpp    # 多标签管理器
│   │   └── ThemeManager.h/cpp  # 主题管理
│   └── utils/                  # 工具
│       └── Logger.h/cpp        # 日志系统
│
├── resources/                  # 资源文件
│   ├── resources.qrc           # Qt 资源定义
│   ├── themes/                 # CSS 主题
│   │   ├── light.css           # 浅色主题
│   │   └── dark.css            # 深色主题
│   └── backgrounds/            # 背景图片
│
└── build/                      # 构建目录
    └── DocMindAI.exe           # 编译产物
```

## 编译与运行

### 环境要求

- **CMake** 3.20+
- **C++20** 编译器（GCC 13+ / MSVC 2022+）
- **Qt 6.5+**（Widgets, Network, PrintSupport）

### 构建步骤

```bash
# 配置
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" \
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"

# 编译
ninja

# 运行
./DocMindAI.exe
```

## 快捷键一览

| 快捷键 | 功能 |
|-------|------|
| Ctrl+N | 新建文件 |
| Ctrl+O | 打开文件 |
| Ctrl+S | 保存 |
| Ctrl+Shift+S | 另存为 |
| Ctrl+W | 关闭当前标签 |
| Ctrl+Z / Y | 撤销 / 重做 |
| Ctrl+F | 查找 |
| Ctrl+H | 查找替换 |
| Ctrl+G | 跳转到行 |
| Ctrl+B | 粗体 |
| Ctrl+I | 斜体 |
| Ctrl+K | 插入链接 |
| Ctrl+Shift+K | 插入代码块 |
| Ctrl+Shift+T | 切换主题 |
| Ctrl+Shift+L | 显示/隐藏行号 |
| Ctrl+滚轮 | 缩放字体 |

## 模块进度

| 模块 | 描述 | 状态 |
|-----|------|------|
| **A — 编辑与预览核心** | 编辑器、预览、文档管理 | ✅ 完成 |
| B — 文档转换中枢 | DOCX/PDF/HTML 导入导出 | ⬜ 计划中 |
| C — AI 服务与数据管理 | OpenAI API、知识库 | ⬜ 计划中 |

## 技术栈

- **语言**: C++20
- **GUI**: Qt 6.11 Widgets
- **构建**: CMake + Ninja + MinGW
- **渲染**: 自研 Markdown→HTML 引擎 + QTextBrowser
- **高亮**: 自研 QSyntaxHighlighter
- **存储**: JSON 配置文件 + 文件系统

## 许可证

MIT License
