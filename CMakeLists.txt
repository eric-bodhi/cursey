cmake_minimum_required(VERSION 3.26)
project(TextEditor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile_commands for easier debugging
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the externals directory for header files
include_directories(${CMAKE_SOURCE_DIR}/externals/gap-buffer/src)

# Add the main executable target
add_executable(main
  main.cpp
  src/tui.cpp
  src/textbuffer.cpp
  src/editor.cpp
  src/log.cpp
)

# Clone gap-buffer repository manually into externals/gap-buffer
# You can either clone this manually, or you can also use Git to pull the repository automatically (but not required for this setup).
# If you want to automate the clone (in case it's not already done), you can uncomment below line:
#
execute_process(COMMAND git clone git@github.com:eric-bodhi/gap-buffer.git ${CMAKE_SOURCE_DIR}/externals/gap-buffer)

# In this setup, the gap-buffer repo should already be in the 'externals' folder.
# You can manually clone the repo using:
# git clone git@github.com:eric-bodhi/gap-buffer.git externals/gap-buffer
#
# This will put the gap-buffer project inside your 'externals' folder and use the header files from there.
