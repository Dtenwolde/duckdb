add_library_unity(
        duckdb_peg_parser
        OBJECT
        peg_parser.cpp)
set(PEG_PARSER_EXTENSION_FILES
        ${PEG_PARSER_EXTENSION_FILES} $<TARGET_OBJECTS:duckdb_peg_parser>
        PARENT_SCOPE)
