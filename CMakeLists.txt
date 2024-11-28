cmake_minimum_required(VERSION 3.26)
project(TextEditor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(main
main.cpp
src/tui.cpp
src/textbuffer.cpp
src/editor.cpp
src/log.cpp)

