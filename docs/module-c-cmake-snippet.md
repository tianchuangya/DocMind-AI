# 模块 C — CMake 集成片段

> 归档说明：本文是模块 C 合并前的 CMake 对接草稿。当前 `main` 分支的根 `CMakeLists.txt` 已经接入模块 C 的实际源文件、Qt Sql 和 Qt Concurrent；正式构建配置请以根目录 `CMakeLists.txt` 为准。

合并到主分支时,把以下改动贴进根 `CMakeLists.txt`。

## 1. find_package 增加 Sql 与 Concurrent

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network PrintSupport Sql Concurrent
)
```

- `Sql`:模块 C 的 `KnowledgeRepository`、`SettingsRepository`、`DbMigrator` 依赖。
- `Concurrent`:`QtConcurrent::run`(用于异步嵌入、入库)。
- `Network`:模块 A 已有;模块 C 的 `OpenAICompatibleProvider` 也需要,确认保留。

## 2. target_link_libraries 增加

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::PrintSupport
    Qt6::Sql Qt6::Concurrent
)
```

## 3. APP_SOURCES 追加(模块 C 源文件,实现阶段逐个填入)

```cmake
set(APP_SOURCES
    # ...模块 A/B 现有源文件...

    # 模块 C
    src/ai/OpenAICompatibleProvider.cpp
    src/ai/WritingAssistant.cpp
    src/knowledge/ChunkingStrategy.cpp
    src/knowledge/KnowledgeRepository.cpp
    src/knowledge/KnowledgeIngestionService.cpp
    src/knowledge/KnowledgeQueryService.cpp
    src/storage/SecureCredentialStore.cpp
    src/storage/SettingsRepository.cpp
    src/storage/DbMigrator.cpp
    # src/sync/LocalOnlySyncProvider.h 是 header-only,无需加入
)
```

## 4. APP_HEADERS 追加

```cmake
set(APP_HEADERS
    # ...模块 A/B 现有头文件...

    # 模块 C
    src/ai/AIProvider.h
    src/ai/OpenAICompatibleProvider.h
    src/ai/WritingAssistant.h
    src/knowledge/KnowledgeTypes.h
    src/knowledge/ChunkingStrategy.h
    src/knowledge/KnowledgeRepository.h
    src/knowledge/KnowledgeIngestionService.h
    src/knowledge/KnowledgeQueryService.h
    src/storage/SecureCredentialStore.h
    src/storage/SettingsRepository.h
    src/storage/DbMigrator.h
    src/sync/SyncProvider.h
    src/sync/LocalOnlySyncProvider.h
)
```

## 5. 平台依赖(SecureCredentialStore 实现)

`SecureCredentialStore` 在不同平台需要链接不同的系统库,实现阶段追加:

```cmake
if(APPLE)
    find_library(SECURITY_FRAMEWORK Security)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_FRAMEWORK})
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE advapi32 credmgr)
elseif(UNIX AND NOT APPLE)
    # libsecret 通常通过 pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
        if(LIBSECRET_FOUND)
            target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBSECRET)
        endif()
    endif()
endif()
```

首期若 libsecret 不可用,会自动回退到加密文件实现,因此 `LIBSECRET_FOUND` 是可选的。

---

## 当前状态

模块 C **只有头文件**,`.cpp` 尚未实现。CMake 片段里的 `APP_SOURCES` 列出的 `.cpp` 在实现阶段才存在。集成时若某 `.cpp` 还未写,从列表里去掉对应行即可(头文件无此问题)。
