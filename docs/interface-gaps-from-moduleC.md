# 模块 C → 模块 A 接口对齐清单

- **来源**:模块 C(AI 服务与知识库)负责人
- **目标**:模块 A(编辑与预览核心)负责人
- **分支对齐基准**:`feat/initial-module-a`(2026-06-24 核对)
- **目的**:模块 C 即将开始实现,需要模块 A 补齐以下接口或确认边界,避免后期合并冲突与返工。

---

## 一、必须新增的接口(不加则模块 C 核心流程无法实现)

### 1. 选区替换 / 光标处插入

**现状**:
- `DocumentSession::insertText(text)` 实现为 `m_content += text`,即**追加到文末**,不是光标处插入。
- `MarkdownEditor` 有 `selectedText()`(只读),但**没有**对应的 `replaceSelection` / `insertAtCursor`。

**写作助手场景受影响**:续写、改写、润色、翻译等结果需要"用户确认后替换选区"或"在光标处插入",目前无入口。

**需要(建议放 `MarkdownEditor`,因为光标/选区是编辑器概念)**:
```cpp
// src/editor/MarkdownEditor.h
public:
    /// 替换当前选区;无选区时在光标处插入
    void replaceSelection(const QString& text);
    /// 在光标处插入(不依赖选区)
    void insertAtCursor(const QString& text);
```

**若 `DocumentSession` 也要支持非编辑器路径(例如导出回写)**:
```cpp
// src/core/DocumentSession.h
public:
    void replaceRange(int start, int end, const QString& text);
```

### 2. `DocumentSnapshot` 携带选区信息

**现状**:`DocumentSnapshot` 只有 `content / filePath / title / baseDir / renderVersion / isModified`,无选区字段。模块 C 想只消费快照(符合 PLAN "预览和导出只消费其稳定快照"),但拿不到选区。

**需要补字段**:
```cpp
// src/core/DocumentSession.h
struct DocumentSnapshot {
    // ...existing fields...
    int      selectionStart = -1;   // -1 表示无选区
    int      selectionEnd   = -1;
    QString  selectedText;          // 可为空
};
```

### 3. AI 配置的存放位置确认

**现状**:`AppState` 无 Base URL / API Key / 模型名等 AI 字段。

**模块 C 的建议**:
- AI 配置由模块 C 自建 `SettingsRepository`(SQLite 普通字段)+ `SecureCredentialStore`(系统钥匙串,存 API Key),**不扩展 `AppState`**。
- `AppState` 保持只管 UI / 编辑器 / 窗口状态。
- **请模块 A 确认**:接受此边界?若不接受,请在 `AppState` 预留 AI 配置字段。

**附带问题**:`AppState::configDir()` 返回的目录是否已 `mkpath` 保证存在?模块 C 要在此放 `knowledge.db`,需要确认目录一定可写。

---

## 二、需要模块 A 预留的 UI 位置

### 4. AI 侧栏 dock 槽位

PLAN 写"AI 侧栏",模块 C 会实现 `AiPanelWidget`(写作助手 + 知识库问答入口)。需要模块 A 的 `MainWindow` 预留一个右侧 `QDockWidget` 位置 —— 或者明确:由模块 C 自行 `addDockWidget`?

**需要明确**:
- dock 由谁创建并注册到 `MainWindow`?
- 菜单/工具栏的"AI 面板"开关按钮由谁加?

### 5. 知识库管理面板入口

模块 C 会实现 `KnowledgeManagerWidget`(导入资料、查看分块、删除/重建索引)。需要入口位置:
- 菜单项 "视图 → 知识库" 或 "工具 → 知识库管理"
- 建议模块 A 在菜单结构里预留位置,模块 C 填充 action。

---

## 三、构建侧需要协调的改动

### 6. CMakeLists 增加 `Qt6::Sql`

**现状**:
```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network PrintSupport)
```
缺 `Sql`。

**模块 C 必须**:SQLite(知识库元数据、FTS5 全文索引、向量存储)依赖 `Qt6::Sql`。

**建议改动**(由模块 A 落地,避免两个分支合并时 CMakeLists 冲突):
```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network PrintSupport Sql)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::PrintSupport Qt6::Sql
)
```

### 7. WebEngine(可选,需模块 A 决策)

模块 A 开发日志提到"未安装 WebEngine 模块,暂用 QTextBrowser"。模块 C **不直接依赖** WebEngine,但如果将来 AI 来源引用需要富文本高亮渲染,会跟随模块 A 的预览方案。

**需要模块 A 决策**:是否引入 `Qt6::WebEngineWidgets`?模块 C 跟随。

---

## 四、可复用、无需改动的现有接口(已对齐,仅供模块 A 知悉)

| 模块 C 用途 | 调用 | 状态 |
|---|---|---|
| 取当前文档会话 | `TabManager::currentSession()` | ✅ |
| 取当前编辑器 | `TabManager::currentEditor()` | ✅ |
| 取全文 | `DocumentSession::content()` / `snapshot().content` | ✅ |
| 取选区文本(只读) | `MarkdownEditor::selectedText()` | ✅ |
| 取光标位置 | `DocumentSession::cursorLine()/cursorColumn()` | ✅ |
| 日志 | `LOG_INFO("AI", ...)` / `LOG_ERROR("Knowledge", ...)` | ✅ |
| 配置目录 | `AppState::configDir()` | ⚠️ 需确认是否 mkpath(见 #3) |
| 命名空间 | `dmc`(模块 C 全部使用) | ✅ |
| 当前标签切换信号 | `TabManager::currentTabChanged(DocumentSession*)` | ✅ 模块 C 监听做"AI 上下文随标签刷新" |

---

## 五、建议模块 A 顺带修的问题(非阻塞)

### 8. `DocumentSession::insertText` 语义

当前实现是 `m_content += text`(追加到文末),与命名 "insert" 不符,容易误导调用方。建议:
- 改为光标处插入(委托 `MarkdownEditor`),或
- 重命名为 `appendText`。

模块 C 不依赖此函数(模块 C 用自己的 `insertAtCursor`),仅为代码整洁性建议。

---

## 六、模块 C 的目录与命名空间规划(供模块 A 知悉,避免冲突)

```
src/ai/            # AIProvider, OpenAICompatibleProvider, ChatRequest/StreamEvent, EmbeddingRequest
src/knowledge/     # KnowledgeRepository, IngestionService, QueryService, 分块/嵌入
src/storage/       # SettingsRepository, SecureCredentialStore (钥匙串), DbMigrator
src/sync/          # SyncProvider (仅接口,不实现)
```

所有类位于 `dmc` 命名空间。模块 C 不会修改 `src/core/`、`src/editor/`、`src/preview/`、`src/widgets/`、`src/app/` 下的现有文件(除 CMakeLists 协调项 #6)。

---

## 待确认事项汇总(请模块 A 回复)

| # | 事项 | 类型 |
|---|---|---|
| 1 | 是否新增 `MarkdownEditor::replaceSelection/insertAtCursor` | 必需 |
| 2 | 是否给 `DocumentSnapshot` 加选区字段 | 必需 |
| 3 | AI 配置边界(模块 C 自建 vs 扩展 AppState)+ configDir 是否 mkpath | 必需 |
| 4 | AI 侧栏 dock 由谁创建 | 必需 |
| 5 | 知识库面板菜单入口位置 | 必需 |
| 6 | CMakeLists 加 `Qt6::Sql` 由谁落地 | 必需 |
| 7 | 是否引入 WebEngine | 决策 |
| 8 | `insertText` 语义是否修正 | 非阻塞 |
