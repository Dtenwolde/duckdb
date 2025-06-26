#include "transformer/peg_transformer.hpp"

#include "inlined_grammar.hpp"
#include "matcher.hpp"
#include "parser/peg_parser.hpp"

namespace duckdb {


// void PEGTransformer::AddRule(const string &rule_name, TransformFunction function) {
// 	rules[rule_name] = std::move(function);
// }

reference<ParseResult> PEGTransformer::RootTransformer(PEGTransformerState &state, Allocator &allocator) {
	PEGTransformerFactory factory;
	return factory.CreateTransformer(const_char_ptr_cast(INLINED_PEG_GRAMMAR), "Statement", state, allocator);
}

reference<ParseResult> PEGTransformer::Transform(const string_t &rule_name) {
	auto it = rules.find(rule_name);
	if (it == rules.end()) {
		throw InternalException("No transformer rule found for '%s'", rule_name);
	}
	return it->second(*this);
}


reference<ParseResult> PEGTransformerFactory::CreateTransformer(PEGParser &parser, const string_t &rule_name,
	PEGTransformerState &state, Allocator &allocator) {
	auto entry = parser.rules.find(rule_name);
	if (entry == parser.rules.end()) {
		throw InternalException("No rule found for '%s'", rule_name);
	}

	auto func_entry = transform_functions.find(rule_name);
	if (func_entry == transform_functions.end()) {
		throw InternalException("No transformer registered for rule '%s'", rule_name.GetString());
	}

	PEGTransformer transformer(allocator, state, transform_functions);

	return transformer.Transform(rule_name);

	// Create a new transformer instance for this invocation
	// auto &transformer = make_uniq<PEGTransformer>();

	// Match phase (reuses matcher logic if available)
	// For now, this is hand-wired: "USE" + DottedIdentifier
	// transformer.current_parse_result = transformer.Make<ListParseResult>();

	// Then call the correct transformer
	// reference<ParseResult> result = func_entry->second(transformer);
	// Save or return the transformer as needed
	// return transformer;
}

// PEGTransformer &PEGTransformerFactory::CreateTransformer(PEGParser &parser, string_t rule_name) {
// 	vector<reference<ParseResult>> parameters;
// 	return CreateTransformer(parser, rule_name, parameters);
// }

reference<ParseResult> PEGTransformerFactory::CreateTransformer(const char* grammar, const char* root_rule, PEGTransformerState &state, Allocator &allocator) {
	// parse the grammar into a set of rules
	PEGParser parser;
	parser.ParseRules(grammar);

	return CreateTransformer(parser, root_rule, state, allocator);
}



}