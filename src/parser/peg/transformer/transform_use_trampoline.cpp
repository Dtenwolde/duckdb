#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/sql_statement.hpp"

namespace duckdb {

// Statement <- ... (choice of all statement types)
void PEGTransformerFactory::T_TransformStatement(PEGTransformer &t, TransformerStackFrame &current) {
	auto &choice_pr = current.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	t.PushChoiceFrame(choice_pr, current);
}

void PEGTransformerFactory::R_TransformStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	auto results = frame.GetChoiceResult(choice_pr);
	frame.SetParentResult(std::move(results[0]));
}

// UseStatement <- 'USE' UseTarget
void PEGTransformerFactory::T_TransformUseStatement(PEGTransformer &t, TransformerStackFrame &current) {
	auto &list_pr = current.parse_result->Cast<ListParseResult>();
	t.PushFrame(list_pr.GetChild(1), current);
}

void PEGTransformerFactory::R_TransformUseStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto qn = CastResult<QualifiedName>(std::move(frame.child_results["UseTarget"][0]));
	string value_str;
	if (IsInvalidSchema(qn.schema)) {
		value_str = SQLIdentifier::ToString(qn.name);
	} else {
		value_str = SQLIdentifier::ToString(qn.schema) + "." + SQLIdentifier::ToString(qn.name);
	}
	auto value_expr = make_uniq<ConstantExpression>(Value(value_str));
	auto stmt = make_uniq<SetVariableStatement>("schema", std::move(value_expr), SetScope::AUTOMATIC);
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(std::move(stmt)));
}

// UseTarget <- UseTargetCatalogSchema / SchemaName / CatalogName
void PEGTransformerFactory::T_TransformUseTarget(PEGTransformer &t, TransformerStackFrame &current) {
	auto &choice_pr = current.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	if (choice_pr.GetResult().type != ParseResultType::IDENTIFIER) {
		t.PushChoiceFrame(choice_pr, current);
	}
}

void PEGTransformerFactory::R_TransformUseTarget(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	if (choice_pr.GetResult().type == ParseResultType::IDENTIFIER) {
		QualifiedName result;
		result.name = choice_pr.GetResult().Cast<IdentifierParseResult>().identifier;
		frame.SetParentResult(MakeResult<QualifiedName>(std::move(result)));
	} else {
		auto results = frame.GetChoiceResult(choice_pr);
		frame.SetParentResult(std::move(results[0]));
	}
}

// UseTargetCatalogSchema <- CatalogName '.' ReservedSchemaName ('.' Identifier)*
void PEGTransformerFactory::T_TransformUseTargetCatalogSchema(PEGTransformer &t, TransformerStackFrame &current) {
}

void PEGTransformerFactory::R_TransformUseTargetCatalogSchema(PEGTransformer &t, TransformerStackFrame &frame) {
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
	frame.SetParentResult(MakeResult<QualifiedName>(std::move(result)));
}

} // namespace duckdb