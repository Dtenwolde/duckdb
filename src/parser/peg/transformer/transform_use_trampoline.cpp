#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/sql_statement.hpp"

namespace duckdb {

// Statement <- ... (choice of all statement types)
void PEGTransformerFactory::T_TransformStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	t.PushChoiceFrame(choice_pr, frame);
}

void PEGTransformerFactory::R_TransformStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	frame.SetParentResult(frame.GetChoiceResult(choice_pr));
	t.PopFrame();
}

// UseStatement <- 'USE' UseTarget
void PEGTransformerFactory::T_TransformUseStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	QualifiedName qn;
	// child 0 is 'USE' keyword, child 1 is UseTarget
	// if (!t.PushAndAwait<QualifiedName>(frame, list_pr.GetChild(1), qn)) {
	// 	return;
	// }

	string value_str;
	if (IsInvalidSchema(qn.schema)) {
		value_str = KeywordHelper::WriteOptionallyQuoted(qn.name);
	} else {
		value_str =
		    KeywordHelper::WriteOptionallyQuoted(qn.schema) + "." + KeywordHelper::WriteOptionallyQuoted(qn.name);
	}

	auto value_expr = make_uniq<ConstantExpression>(Value(value_str));
	auto stmt = make_uniq<SetVariableStatement>("schema", std::move(value_expr), SetScope::AUTOMATIC);
	// t.SetParentResult(t.MakeStatementResult(std::move(stmt)));
	t.PopFrame();
}

// UseTarget <- UseTargetCatalogSchema / SchemaName / CatalogName
void PEGTransformerFactory::T_TransformUseTarget(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);

	if (choice_pr.GetResult().type == ParseResultType::IDENTIFIER) {
		QualifiedName result;
		result.name = choice_pr.GetResult().Cast<IdentifierParseResult>().identifier;
		// t.SetParentResult(MakeResult<QualifiedName>(std::move(result)));
		t.PopFrame();
		return;
	}

	// if (!t.PushAndForward(frame, choice_pr.GetResult())) {
	// 	return;
	// }
}

// UseTargetCatalogSchema <- CatalogName '.' ReservedSchemaName ('.' Identifier)*
void PEGTransformerFactory::T_TransformUseTargetCatalogSchema(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	auto catalog = list_pr.Child<IdentifierParseResult>(0).identifier;
	auto schema = list_pr.Child<IdentifierParseResult>(2).identifier;
	auto &extra_opt = list_pr.Child<OptionalParseResult>(3);
	if (extra_opt.HasResult()) {
		throw ParserException("Expected \"USE database\" or \"USE database.schema\"");
	}
	QualifiedName result;
	result.catalog = INVALID_CATALOG;
	result.schema = catalog;
	result.name = schema;
	// t.SetParentResult(MakeResult<QualifiedName>(std::move(result)));
	t.PopFrame();
}

} // namespace duckdb