cmake_minimum_required(VERSION 3.16)
project(DocMindAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(ConversionService
    src/conversion/ConversionService.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(ConversionService PRIVATE Threads::Threads)

if(APPLE)
    target_link_libraries(ConversionService PRIVATE "-framework CoreFoundation")
endif()

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

message(STATUS "DocMindAI ConversionService v${PROJECT_VERSION} — C++20")
