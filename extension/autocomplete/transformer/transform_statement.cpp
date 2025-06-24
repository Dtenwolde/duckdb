#include "duckdb/common/printer.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<ParseResult> PEGTransformer::TransformStatement(ParseResult &input) {
	Printer::Print("Got here");
	return {};
}
} // namespace duckdb
