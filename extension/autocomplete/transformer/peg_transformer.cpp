#include "transformer/peg_transformer.hpp"

#include "duckdb/parser/statement/set_statement.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/printer.hpp"

namespace duckdb {

template <typename T>
T PEGTransformer::Transform(ParseResult &parse_result) {
	auto it = transform_functions.find(parse_result.name);
	if (it == transform_functions.end()) {
		throw NotImplementedException("No transformer function found for rule '%s'", parse_result.name);
	}
	auto &func = it->second;

	unique_ptr<TransformResultValue> base_result = func(*this, parse_result);
	if (!base_result) {
		throw InternalException("Transformer for rule '%s' returned a nullptr.", parse_result.name);
	}

	auto *typed_result_ptr = dynamic_cast<TypedTransformResult<T> *>(base_result.get());
	if (!typed_result_ptr) {
		throw InternalException("Transformer for rule '" + parse_result.name + "' returned an unexpected type.");
	}

	return std::move(typed_result_ptr->value);
}

template <typename T>
T PEGTransformer::TransformEnum(ParseResult &parse_result) {
	const string_t &enum_rule_name = parse_result.name;
	if (enum_rule_name.GetString().empty()) {
		throw InternalException("TransformEnum called on a ParseResult with no name.");
	}

	string_t matched_option_name;

	if (parse_result.type == ParseResultType::CHOICE) {
		auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
		matched_option_name = choice_pr.result.get().name;
	} else {
		matched_option_name = parse_result.name;
	}

	if (matched_option_name.GetString().empty()) {
		throw ParserException("Enum transform failed: could not determine matched rule name.");
	}
	auto &rule_mapping = enum_mappings.at(enum_rule_name.GetString());
	auto it = rule_mapping.find(matched_option_name.GetString());
	if (it == rule_mapping.end()) {
		throw ParserException("Enum transform failed: could not map rule '%s' for enum '%s'",
		                      matched_option_name.GetString(), enum_rule_name.GetString());
	}

	auto *typed_enum_ptr = dynamic_cast<TypedTransformEnumResult<T> *>(it->second.get());
	if (!typed_enum_ptr) {
		throw InternalException("Enum mapping for rule '%s' has an unexpected type.", enum_rule_name.GetString());
	}

	return typed_enum_ptr->value;
}

} // namespace duckdb
