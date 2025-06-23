#include "transformer/peg_transformer.hpp"

namespace duckdb {


void PEGTransformer::AddRule(const string &rule_name, TransformFunction function) {
	rules[rule_name] = std::move(function);
}

unique_ptr<ParseResult> PEGTransformer::Transform(const string &rule_name, ParseResult &input) {
	auto it = rules.find(rule_name);
	if (it == rules.end()) {
		throw InternalException("No transformer rule found for '%s'", rule_name);
	}
	return it->second(*this, input);
}



}