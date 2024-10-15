cmake_minimum_required(VERSION 3.14)
project(peglint)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executable for peglint
add_executable(peglint peglint.cc)

# Add the path to the PEG parser library
target_include_directories(peglint PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/peg_parser
)

# Optionally set optimization flags and warnings (based on your Makefile)
target_compile_options(peglint PRIVATE -O3 -Wall)

# Optionally link any additional libraries (if needed)
# target_link_libraries(peglint ${add_link_deps})