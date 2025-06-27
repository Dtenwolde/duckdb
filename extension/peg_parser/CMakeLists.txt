cmake_minimum_required(VERSION 2.8.12...3.29)

project(PegParserExtension)

include_directories(include)

set(PEG_PARSER_EXTENSION_FILES peg_parser_extension.cpp peg_parser_override.cpp tokenizer.cpp)

add_subdirectory(parser)
add_subdirectory(transformer)

build_static_extension(peg_parser ${PEG_PARSER_EXTENSION_FILES})
set(PARAMETERS "-warnings")
build_loadable_extension(peg_parser ${PARAMETERS}
        ${PEG_PARSER_EXTENSION_FILES})

install(
        TARGETS peg_parser_extension
        EXPORT "${DUCKDB_EXPORT_SET}"
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
