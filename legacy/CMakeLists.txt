cmake_minimum_required(VERSION 3.26)
project(cursey LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(CURSEY_BUILD_TESTS "Build the unit tests" ON)
option(CURSEY_SANITIZE "Build with AddressSanitizer + UBSan" OFF)

set(CURSEY_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                    -Wno-sign-conversion)
if(CURSEY_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# Editor logic. Has no terminal dependency, so tests can link it directly.
add_library(cursey_core STATIC
  src/core/buffer.cpp
  src/core/editor.cpp
  src/core/keys.cpp
  src/core/motions.cpp
  src/core/options.cpp
  src/core/registers.cpp
  src/commands/ex_commands.cpp
  src/utils/log.cpp
  src/utils/utf8.cpp
)
target_include_directories(cursey_core PUBLIC src)
target_compile_options(cursey_core PRIVATE ${CURSEY_WARNINGS})

# Terminal front end.
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses)

add_executable(cursey
  main.cpp
  src/ui/tui.cpp
  src/ui/lex.cpp
)
target_link_libraries(cursey PRIVATE cursey_core PkgConfig::NOTCURSES)
target_compile_options(cursey PRIVATE ${CURSEY_WARNINGS})

if(CURSEY_BUILD_TESTS)
  enable_testing()
  add_executable(cursey_tests
    tests/test_main.cpp
    tests/buffer_tests.cpp
    tests/motion_tests.cpp
    tests/editor_tests.cpp
  )
  target_link_libraries(cursey_tests PRIVATE cursey_core)
  target_compile_options(cursey_tests PRIVATE ${CURSEY_WARNINGS})
  add_test(NAME cursey_tests COMMAND cursey_tests)
endif()

# syntax.cpp: experimental libclang-based highlighter (not built by default).
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
