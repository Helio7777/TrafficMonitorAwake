cmake_minimum_required(VERSION 3.20)
project(TrafficMonitorAwake VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "TrafficMonitorAwake is a Windows-only plugin.")
endif()

add_library(TrafficMonitorAwake SHARED
    src/AwakeManager.cpp
    src/AwakeOptionsDialog.cpp
    src/AwakePlugin.cpp
)

target_include_directories(TrafficMonitorAwake PRIVATE src)
target_compile_features(TrafficMonitorAwake PRIVATE cxx_std_17)
target_compile_definitions(TrafficMonitorAwake PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0601
    _WIN32_WINNT=0x0601
)

target_link_libraries(TrafficMonitorAwake PRIVATE Comctl32 gdiplus)

if(MSVC)
    target_compile_options(TrafficMonitorAwake PRIVATE /W4 /permissive- /utf-8 /EHsc)
endif()

set_target_properties(TrafficMonitorAwake PROPERTIES
    PREFIX ""
    OUTPUT_NAME "TrafficMonitorAwake"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
