cmake_minimum_required(VERSION 3.20)
project(DocMind-AI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt 6 依赖 ────────────────────────────────────────────
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Sql          # 模块 C SQLite 依赖
    Concurrent   # QtConcurrent::run 异步提取
)

if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

# ── 源文件 ───────────────────────────────────────────────
set(SOURCES
    src/main.cpp

    # 核心类型
    src/conversion/Types.cpp

    # 工具 & 进程封装
    src/conversion/ProcessRunner.cpp
    src/conversion/ToolRunner.cpp

    # Markdown 解析
    src/conversion/MarkdownParser.cpp

    # 外部工具转换器（Pandoc / Tectonic / Poppler）
    src/conversion/PandocConverter.cpp
    src/conversion/TectonicConverter.cpp
    src/conversion/PopplerExtractor.cpp

    # 资源管理 & 诊断
    src/conversion/ResourceManager.cpp
    src/conversion/Diagnostics.cpp

    # 引擎 & 服务
    src/conversion/ConversionEngine.cpp
    src/conversion/ConversionService.cpp

    # 原生转换器（不依赖外部工具）
    src/conversion/native/NativeMarkdownConverter.cpp
    src/conversion/native/NativeDocxConverter.cpp
    src/conversion/native/NativePdfConverter.cpp
    src/conversion/native/NativeConversionService.cpp
)

set(HEADERS
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
    src/conversion/native/NativeDocxConverter.h
    src/conversion/native/NativePdfConverter.h
    src/conversion/native/NativeConversionService.h
)

# ── 构建目标 ─────────────────────────────────────────────
add_executable(DocMind-AI ${SOURCES} ${HEADERS})

target_link_libraries(DocMind-AI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::CorePrivate      # QZipReader/QZipWriter（原生 DOCX）
    z                      # zlib（原生 PDF 流解压）
)

target_include_directories(DocMind-AI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── 编译选项 ─────────────────────────────────────────────
if(MSVC)
    target_compile_options(DocMind-AI PRIVATE /W4)
else()
    target_compile_options(DocMind-AI PRIVATE -Wall -Wextra -Wpedantic)
endif()

# macOS: AGL 框架在 macOS 26 SDK 中已移除，使用 stub 解决 Qt 遗留依赖
if(APPLE)
    if(EXISTS "/tmp/AGL_stub/AGL.framework")
        target_link_options(DocMind-AI PRIVATE -F/tmp/AGL_stub)
    endif()
endif()

# ── 安装规则 ─────────────────────────────────────────────
install(TARGETS DocMind-AI
    BUNDLE  DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── 平台配置 ─────────────────────────────────────────────
if(APPLE)
    set_target_properties(DocMind-AI PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.docmind.ai
        MACOSX_BUNDLE_BUNDLE_NAME "DocMind-AI"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

if(WIN32)
    set_target_properties(DocMind-AI PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# ── 输出配置信息 ─────────────────────────────────────────
message(STATUS "DocMind-AI Configuration:")
message(STATUS "  Version:       ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt Version:    ${Qt6_VERSION}")
message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tool-based:    Pandoc / Tectonic / Poppler")
message(STATUS "  Native:        YES (Qt built-in)")
