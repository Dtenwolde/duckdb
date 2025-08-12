#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformDeleteStatement(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	throw NotImplementedException("DELETE statement not implemented.");
}

} // namespace duckdb
