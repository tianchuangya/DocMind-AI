cmake_minimum_required(VERSION 3.20)
project(DocMindAI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Qt6 ─────────────────────────────────────────────────────────────────────
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network Sql Concurrent
)

# ─── 源码 ─────────────────────────────────────────────────────────────────────
set(APP_SOURCES
    # 入口
    src/main.cpp
    # UI（模块 C 自带 demo）
    src/app/DemoWindow.cpp
    # 模块 A（仅 Logger，编译依赖）
    src/utils/Logger.cpp
    # 模块 B（转换服务）
    src/conversion/ConversionService.cpp
    # 模块 C — AI
    src/ai/OpenAICompatibleProvider.cpp
    src/ai/WritingAssistant.cpp
    # 模块 C — 知识库
    src/knowledge/ChunkingStrategy.cpp
    src/knowledge/KnowledgeRepository.cpp
    src/knowledge/KnowledgeIngestionService.cpp
    src/knowledge/KnowledgeQueryService.cpp
    # 模块 C — 存储
    src/storage/DbMigrator.cpp
    src/storage/SecureCredentialStore.cpp
    src/storage/SettingsRepository.cpp
)

set(APP_HEADERS
    src/app/DemoWindow.h
    src/utils/Logger.h
    src/conversion/ConversionService.h
    src/ai/AIProvider.h
    src/ai/OpenAICompatibleProvider.h
    src/ai/WritingAssistant.h
    src/knowledge/KnowledgeTypes.h
    src/knowledge/ChunkingStrategy.h
    src/knowledge/KnowledgeRepository.h
    src/knowledge/KnowledgeIngestionService.h
    src/knowledge/KnowledgeQueryService.h
    src/knowledge/ConversionEngineExtractionAdapter.h
    src/storage/SecureCredentialStore.h
    src/storage/SettingsRepository.h
    src/storage/DbMigrator.h
    src/sync/SyncProvider.h
    src/sync/LocalOnlySyncProvider.h
)

# ─── 目标 ─────────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
)

# ─── 平台依赖：SecureCredentialStore（首期仅加密文件回退，无需原生库） ──────
# 后续接入钥匙串时再追加：
# if(APPLE)
#     find_library(SECURITY_FRAMEWORK Security)
#     target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_FRAMEWORK})
# elseif(WIN32)
#     target_link_libraries(${PROJECT_NAME} PRIVATE advapi32)
# elseif(UNIX AND NOT APPLE)
#     pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
#     if(TARGET PkgConfig::LIBSECRET)
#         target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBSECRET)
#     endif()
# endif()

# ─── 编译选项 ─────────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2>
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060500
)

# ─── 安装 ─────────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)

# ─── Windows 部署 ────────────────────────────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations
                    "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Deploying Qt runtime...")
    endif()
endif()

message(STATUS "DocMindAI v${PROJECT_VERSION} — Qt ${Qt6_VERSION} (module C demo)")
