// transform_use_statement.cpp
#include "transformer/parse_result.hpp"
#include "duckdb/common/helper.hpp"

namespace duckdb {

unique_ptr<ParseResult> PEGTransformer::TransformUseStatement(ParseResult &input) {
	// UseStatement <- 'USE'i DottedIdentifier
	auto &list = input.Cast<ListParseResult>();
	if (list.parse_result.size() != 2) {
		throw InternalException("Invalid number of children for UseStatement");
	}
	auto &first_child = list.GetChild<KeywordParseResult>(0);
	auto &second_child = list.GetChild<ListParseResult>(1);

	return {};
	// Skip the 'USE' keyword
	// auto &keyword = list.parse_result[0].get();
	// if (keyword.type != ParseResultType::KEYWORD) {
		// throw InternalException("Expected KEYWORD as first child in UseStatement");
	// }

	// Second child should be a string literal (DottedIdentifier -> CatalogName or SchemaName -> StringLiteral)
	// auto &identifier = GetChild<IdentifierParseResult>(list.parse_result[1]);

	// auto stmt = make_uniq<UseStatement>();
	// stmt->schema = identifier.identifier;

	// return Make<StatementParseResult>(std::move(stmt));
}

} // namespace duckdb