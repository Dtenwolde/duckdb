#include "transformer/peg_transformer.hpp"

#include "inlined_grammar.hpp"
#include "parser/peg_parser.hpp"

namespace duckdb {


void PEGTransformer::AddRule(const string &rule_name, TransformFunction function) {
	rules[rule_name] = std::move(function);
}

unique_ptr<PEGTransformer> PEGTransformer::RootTransformer() {
	PEGTransformerFactory factory;
	return factory.Create(const_char_ptr_cast(INLINED_PEG_GRAMMAR), "Statement");
}

unique_ptr<ParseResult> PEGTransformer::Transform(const string &rule_name, ParseResult &input) {
	auto it = rules.find(rule_name);
	if (it == rules.end()) {
		throw InternalException("No transformer rule found for '%s'", rule_name);
	}
	return it->second(*this, input);
}

unique_ptr<PEGTransformer> PEGTransformerFactory::Create(const char* grammar, const char* root_rule) {
	auto transformer = make_uniq<PEGTransformer>();
	// parse the grammar into a set of rules
	PEGParser parser;
	parser.ParseRules(grammar);

	transformer->AddRule(root_rule, [](PEGTransformer &self, ParseResult &result) {
		return self.TransformStatement(result);
	});
	transformer->AddRule("UseStatement", [](PEGTransformer &self, ParseResult &result) {
		return self.TransformUseStatement(result);
	});

	return transformer;
}


}