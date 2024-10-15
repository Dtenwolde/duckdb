#include "duckdb/parser/peg_transformer.hpp"

namespace duckdb {

PEGTransformer::PEGTransformer(ParserOptions &options)
        : parent(nullptr), options(options), stack_depth(DConstants::INVALID_INDEX) {
}


} // namespace duckdb
