cmake_minimum_required(VERSION 3.16)
project(clib)

enable_language(C CXX)

# Set the output path for executable binaries (e.g., your main program)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Set the output path for shared and static libraries (optional)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# Set the output path for archive files (e.g., static libraries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/scripts)

include(cmake_helper)

inc_subdir()