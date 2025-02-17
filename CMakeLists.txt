cmake_minimum_required(VERSION 3.26)
project(TextEditor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the main executable target
add_executable(main
  main.cpp
  src/core/tui.cpp
  src/core/buffer.cpp
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

#
#
#

# syntax.cpp testing stuff
#[[
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Add include directories
include_directories(
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
)

# Add compiler flags from LLVM
add_compile_options(${LLVM_CXXFLAGS})

# Create the executable target
add_executable(syntax syntax.cpp)

# Link against required LLVM/Clang components
target_link_libraries(syntax PRIVATE
        clangFrontend
        clangSerialization
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangAST
        clangASTMatchers
        clangEdit
        clangLex
        clangBasic
        LLVMCore
        LLVMSupport
)

# Add linker flags and system libraries
target_link_options(syntax PRIVATE ${LLVM_LDFLAGS})
target_link_libraries(syntax PRIVATE ${LLVM_SYSTEM_LIBS})
]]
