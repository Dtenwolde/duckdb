cmake_minimum_required(VERSION 3.14)
project(peglint)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executable for peglint
add_executable(peglint peglint.cc)

# Include the current directory for headers (peglib.h is in the same directory)
target_include_directories(peglint PRIVATE ${CMAKE_SOURCE_DIR})

# Link any additional libraries here (you mentioned ${add_link_deps}, replace it if needed)
# If you need to add libraries to link against, use:
# target_link_libraries(peglint ${add_link_deps})

# Optionally, set optimization flags and warnings (based on your Makefile)
target_compile_options(peglint PRIVATE -O3 -Wall)