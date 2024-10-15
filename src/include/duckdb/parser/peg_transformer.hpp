#pragma once

#include "duckdb/parser/parser_options.hpp"
namespace duckdb {


class PEGTransformer {

    explicit PEGTransformer(ParserOptions &options);

private:
    optional_ptr<PEGTransformer> parent;
    //! Parser options
    ParserOptions &options;
    //! Current stack depth
    idx_t stack_depth;

};

} // namespace duckdb

