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
	
	result->info = std::move(info);
	return result;
}

string PEGTransformerFactory::TransformAttachAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &col_id = list_pr.Child<ListParseResult>(1).Child<ChoiceParseResult>(0);
	return col_id.result->Cast<IdentifierParseResult>().identifier;
}

} // namespace duckdb
