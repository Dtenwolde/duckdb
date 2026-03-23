#pragma once

#include "transformer/peg_transformer.hpp"
#include "transformer/transform_result.hpp"
namespace duckdb {
//===--------------------------------------------------------------------===//
// Grammar extensions
//===--------------------------------------------------------------------===//
struct GrammarExtension {
	using AnyTransformFunction =
	    std::function<unique_ptr<TransformResultValue>(PEGTransformer &, optional_ptr<ParseResult>)>;
	explicit GrammarExtension();

	const case_insensitive_map_t<AnyTransformFunction> &transform_functions;
	const case_insensitive_map_t<PEGRule> &grammar_rules;
	const case_insensitive_map_t<unique_ptr<TransformEnumValue>> &enum_mappings;
};

} // namespace duckdb
