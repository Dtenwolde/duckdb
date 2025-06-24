#include "transformer/parse_result.hpp"

namespace duckdb {

ParseResult::~ParseResult() {
}

string ParseResult::ToString() {
	return "ParseResult";
}

} // namespace duckdb