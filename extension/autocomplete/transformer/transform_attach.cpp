#include "ast/generic_copy_option.hpp"
#include "duckdb/parser/statement/attach_statement.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformAttachStatement(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto result = make_uniq<AttachStatement>();
	auto info = make_uniq<AttachInfo>();

	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto or_replace = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto if_not_exists = list_pr.Child<OptionalParseResult>(2).HasResult();

	if (or_replace && if_not_exists) {
		throw ParserException("Cannot specify both OR REPLACE and IF NOT EXISTS at the same time");
	}

	if (or_replace) {
		info->on_conflict = OnCreateConflict::REPLACE_ON_CONFLICT;
	} else if (if_not_exists) {
		info->on_conflict = OnCreateConflict::IGNORE_ON_CONFLICT;
	} else {
		info->on_conflict = OnCreateConflict::ERROR_ON_CONFLICT;
	}

	info->path = list_pr.Child<ListParseResult>(4).Child<StringLiteralParseResult>(0).result;
	auto &attach_alias = list_pr.Child<OptionalParseResult>(5);
	info->name = attach_alias.HasResult() ? transformer.Transform<string>(attach_alias.optional_result) : string();
	auto &attach_options = list_pr.Child<OptionalParseResult>(6);
	if (attach_options.HasResult()) {
		info->options = transformer.Transform<unordered_map<string, Value>>(attach_options.optional_result);
	}
	result->info = std::move(info);
	return result;
}

string PEGTransformerFactory::TransformAttachAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &col_id = list_pr.Child<ListParseResult>(1).Child<ChoiceParseResult>(0);
	return col_id.result->Cast<IdentifierParseResult>().identifier;
}

unordered_map<string, Value> PEGTransformerFactory::TransformAttachOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &parens = list_pr.Child<ListParseResult>(0);
	auto &generic_copy_option_list = parens.Child<ListParseResult>(1);
	return transformer.Transform<unordered_map<string, Value>>(generic_copy_option_list);
}

unordered_map<string, Value> PEGTransformerFactory::TransformGenericCopyOptionList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	unordered_map<string, Value> result;
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &list = list_pr.Child<ListParseResult>(0);
	auto &first_element = list.Child<ListParseResult>(0);
	GenericCopyOption copy_option = transformer.Transform<GenericCopyOption>(first_element);
	result[copy_option.name] = copy_option.value;
	auto &extra_elements = list.Child<OptionalParseResult>(1);
	if (extra_elements.HasResult()) {
		// TODO(dtenwolde) implement this
		throw NotImplementedException("Multiple options not implemented yet.");
	}

	return result;
}

GenericCopyOption PEGTransformerFactory::TransformGenericCopyOption(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	GenericCopyOption copy_option;

	auto &list_pr = parse_result->Cast<ListParseResult>();
	copy_option.name = StringUtil::Lower(list_pr.Child<IdentifierParseResult>(0).identifier);
	auto &optional_expression = list_pr.Child<OptionalParseResult>(1);
	if (optional_expression.HasResult()) {
		auto expression_result = transformer.Transform<unique_ptr<ParsedExpression>>(optional_expression.optional_result);
		auto const_expression = expression_result->Cast<ConstantExpression>();
		copy_option.value = const_expression.value;
	}
	return copy_option;
}

} // namespace duckdb
