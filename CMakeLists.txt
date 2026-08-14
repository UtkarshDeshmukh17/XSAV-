cmake_minimum_required(VERSION 3.20)
project(XSAVAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(WARNING
        "XSAV File Monitor's Windows backend (ReadDirectoryChangesW) "
        "requires Windows. Common interfaces will still compile; the "
        "file_monitor demo executable requires a Windows toolchain.")
endif()

# --- common: Event / IEventSink / EventQueue -------------------------
add_library(xsav_common src/common/Event.cpp)
target_include_directories(xsav_common PUBLIC src)

# --- file_monitor: WindowsFileMonitor ---------------------------------
add_library(xsav_file_monitor src/file_monitor/WindowsFileMonitor.cpp)
target_include_directories(xsav_file_monitor PUBLIC src)
target_link_libraries(xsav_file_monitor PUBLIC xsav_common)

# --- demo executable ----------------------------------------------------
add_executable(xsav_file_monitor_demo src/main.cpp)
target_link_libraries(xsav_file_monitor_demo PRIVATE xsav_file_monitor)

if(WIN32)
    target_compile_definitions(xsav_file_monitor_demo PRIVATE _WIN32_WINNT=0x0601)
endif()
