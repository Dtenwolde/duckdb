#pragma once

#include "duckdb/parser/parser_override.hpp"
#include "transformer/peg_transformer.hpp" // Assumes this is moved here

namespace duckdb {

class PEGParserOverride : public ParserOverride {
public:
	PEGParserOverride();

	vector<unique_ptr<SQLStatement>> Parse(const string &query) override;

private:
	unique_ptr<PEGTransformerFactory> factory;
};

} // namespace duckdb