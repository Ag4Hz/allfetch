cmake_minimum_required(VERSION 3.20)

project(allfetch
    VERSION 0.1.0
    DESCRIPTION "CLI fetch utility with some easter eggs and other fun functionalities"
    LANGUAGES CXX
)

add_executable(allfetch
    src/main.cpp
)

set_target_properties(allfetch PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)