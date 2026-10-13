cmake_minimum_required(VERSION 3.20)
project(influx_stats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.78 REQUIRED)
find_package(Threads REQUIRED)

# Pure logic, no SDK or network: unit-testable on their own.
add_library(influx_line_protocol STATIC src/line_protocol.cpp)
target_include_directories(influx_line_protocol PUBLIC src)

add_library(influx_settings STATIC src/influx_settings.cpp)
target_include_directories(influx_settings PUBLIC src)

add_library(influx_writer STATIC src/influx_writer.cpp)
target_link_libraries(influx_writer PUBLIC influx_line_protocol influx_settings Threads::Threads)

add_library(influx_http STATIC src/http_transport.cpp)
target_link_libraries(influx_http PUBLIC influx_writer Boost::headers Threads::Threads)
if(WIN32)
    target_link_libraries(influx_http PUBLIC ws2_32 mswsock)
endif()

add_library(influx_server_metrics STATIC src/server_metrics.cpp)
target_link_libraries(influx_server_metrics PUBLIC influx_writer)

# Everything the module and other modules need to write points. Linking this does not pull in the SDK.
add_library(influx_logging INTERFACE)
target_link_libraries(influx_logging INTERFACE influx_http influx_server_metrics)

# Added as a subdirectory of another project: provide the library and stop.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    return()
endif()

# The SDK can come from a local checkout (FOREVER_MODULE_SDK_SOURCE) or from GitHub.
set(FOREVER_MODULE_SDK_SOURCE "" CACHE PATH "Path to a forever-module-sdk checkout (optional)")
set(FOREVER_MODULE_SDK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(FOREVER_MODULE_SDK_SOURCE)
    add_subdirectory("${FOREVER_MODULE_SDK_SOURCE}" "${CMAKE_BINARY_DIR}/forever-module-sdk")
else()
    include(FetchContent)
    FetchContent_Declare(forever_module_sdk
        GIT_REPOSITORY https://github.com/Trixx34/forever-module-sdk
        GIT_TAG main)
    FetchContent_MakeAvailable(forever_module_sdk)
endif()

add_executable(influx-stats src/main.cpp)
target_link_libraries(influx-stats PRIVATE influx_http influx_server_metrics forever_module_sdk)

enable_testing()
foreach(name line_protocol settings writer http server_metrics)
    add_executable(${name}_test tests/${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE influx_http influx_server_metrics)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()
