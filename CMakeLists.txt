cmake_minimum_required(VERSION 3.16)
project(DocMindAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network PrintSupport Sql Concurrent
)

set(APP_SOURCES
    src/conversion/ConversionService.cpp
)

set(APP_HEADERS
    src/conversion/ConversionService.h
)

add_executable(ConversionService
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_link_libraries(ConversionService PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::PrintSupport
    Qt6::Sql
    Qt6::Concurrent
)

target_include_directories(ConversionService PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(ConversionService PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

target_compile_definitions(ConversionService PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="ConversionService"
)

install(TARGETS ConversionService
    RUNTIME DESTINATION bin
)

message(STATUS "DocMindAI ConversionService v${PROJECT_VERSION} — Qt6")
