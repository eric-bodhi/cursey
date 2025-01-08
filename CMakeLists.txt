cmake_minimum_required(VERSION 3.26)
project(TextEditor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile_commands for easier debugging
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the main executable target
add_executable(main
  main.cpp
  src/tui.cpp
  src/textbuffer.cpp
  src/editor.cpp
  src/log.cpp
  src/deque_gb.cpp
)
