add_library_unity(
        duckdb_peg_transformer
        OBJECT
        parse_result.cpp
        peg_transformer.cpp
        transform_statement.cpp
        transform_use.cpp)
set(AUTOCOMPLETE_EXTENSION_FILES
        ${AUTOCOMPLETE_EXTENSION_FILES} $<TARGET_OBJECTS:duckdb_peg_transformer>
        PARENT_SCOPE)
