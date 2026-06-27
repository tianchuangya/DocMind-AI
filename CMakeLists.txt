cmake_minimum_required(VERSION 3.20)
project(DocMindAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Qt6 ─────────────────────────────────────────────────────────────────────
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network PrintSupport Sql Concurrent
)

# ─── 源码收集 ─────────────────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/app/SettingsDialog.cpp
    src/core/DocumentSession.cpp
    src/core/AppState.cpp
    src/editor/MarkdownEditor.cpp
    src/editor/MarkdownHighlighter.cpp
    src/editor/LineNumberArea.cpp
    src/editor/FindReplaceDialog.cpp
    src/preview/MarkdownRenderer.cpp
    src/preview/PreviewWidget.cpp
    src/preview/ScrollSyncManager.cpp
    src/widgets/TabManager.cpp
    src/widgets/ThemeManager.cpp
    src/utils/Logger.cpp
    # Module B — conversion service
    src/conversion/Types.cpp
    src/conversion/ProcessRunner.cpp
    src/conversion/ToolRunner.cpp
    src/conversion/MarkdownParser.cpp
    src/conversion/PandocConverter.cpp
    src/conversion/TectonicConverter.cpp
    src/conversion/PopplerExtractor.cpp
    src/conversion/ResourceManager.cpp
    src/conversion/Diagnostics.cpp
    src/conversion/ConversionEngine.cpp
    src/conversion/ConversionService.cpp
    src/conversion/native/NativeMarkdownConverter.cpp
    src/conversion/native/NativePdfConverter.cpp
)

set(APP_HEADERS
    src/app/Application.h
    src/app/MainWindow.h
    src/app/SettingsDialog.h
    src/core/DocumentSession.h
    src/core/AppState.h
    src/editor/MarkdownEditor.h
    src/editor/MarkdownHighlighter.h
    src/editor/LineNumberArea.h
    src/editor/FindReplaceDialog.h
    src/preview/MarkdownRenderer.h
    src/preview/PreviewWidget.h
    src/preview/ScrollSyncManager.h
    src/widgets/TabManager.h
    src/widgets/ThemeManager.h
    src/utils/Logger.h
    # Module B — conversion service
    src/conversion/Types.h
    src/conversion/ProcessRunner.h
    src/conversion/ToolRunner.h
    src/conversion/MarkdownParser.h
    src/conversion/PandocConverter.h
    src/conversion/TectonicConverter.h
    src/conversion/PopplerExtractor.h
    src/conversion/ResourceManager.h
    src/conversion/Diagnostics.h
    src/conversion/ConversionEngine.h
    src/conversion/ConversionService.h
    src/conversion/native/NativeMarkdownConverter.h
    src/conversion/native/NativePdfConverter.h
)

set(APP_RESOURCES
    resources/resources.qrc
)

# ─── 可执行目标 ───────────────────────────────────────────────────────────────
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32
        ${APP_SOURCES}
        ${APP_HEADERS}
        ${APP_RESOURCES}
    )
else()
    add_executable(${PROJECT_NAME}
        ${APP_SOURCES}
        ${APP_HEADERS}
        ${APP_RESOURCES}
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::PrintSupport
    Qt6::Sql
    Qt6::Concurrent
    z
)

# ─── 编译选项 ─────────────────────────────────────────────────────────────────
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060500
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="${PROJECT_NAME}"
)

# ─── 安装规则 ─────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)

# ─── 部署 Qt DLL ─────────────────────────────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations
                    "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Deploying Qt runtime..."
        )
    endif()
endif()

message(STATUS "DocMindAI v${PROJECT_VERSION} — C++20 / Qt ${Qt6_VERSION}")
