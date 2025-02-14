cmake_minimum_required(VERSION 3.26)
project(TextEditor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  src/core/lex.cpp
  src/commands/commands.cpp
  src/keybindings/keybindings.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED notcurses)

target_include_directories(main PRIVATE ${NOTCURSES_INCLUDE_DIRS})
target_link_libraries(main PRIVATE ${NOTCURSES_LIBRARIES})
