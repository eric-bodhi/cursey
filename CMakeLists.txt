cmake_minimum_required(VERSION 3.26)
project(TextEditor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile_commands for easier debugging
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Curses REQUIRED)

# Add the main executable target
add_executable(main
  main.cpp
        src/core/tui.cpp
        src/core/textbuffer.cpp
        src/core/editor.cpp
        src/utils/log.cpp
        src/utils/deque_gb.cpp
        src/core/cursor.cpp
        src/core/viewportmanager.cpp
        src/commands/commands.cpp
        src/keybindings/keybindings.cpp
)

target_link_libraries(main PRIVATE ${CURSES_LIBRARIES})
target_include_directories(main PRIVATE ${CURSES_INCLDE_DIRS})
