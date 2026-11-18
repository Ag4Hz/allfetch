cmake_minimum_required(VERSION 3.20)

project(allfetch
    VERSION 0.1.0
    DESCRIPTION "CLI fetch utility with some easter eggs and other fun functionalities"
    LANGUAGES CXX
)

add_executable(allfetch
    src/main.cpp
    src/core/argparser.cpp
    src/core/command_dispatch.cpp
    src/system/linux/os_info.cpp
    src/system/windows/os_info.cpp
)

target_include_directories(allfetch PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if (WIN32)
    target_sources(allfetch PRIVATE
        src/system/windows/system_info.cpp
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(allfetch PRIVATE
        src/system/linux/system_info.cpp
    )
endif()

set_target_properties(allfetch PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)