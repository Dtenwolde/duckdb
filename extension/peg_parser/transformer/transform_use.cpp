#include "transformer/parse_result.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/statement/set_statement.hpp"

namespace duckdb {

// src/parser/transform/statement/transform_use.cpp
// reference<SetStatement> PEGTransformer::TransformUseStatement(PEGTransformer &self) {
	// UseStatement <- 'USE'i UseTarget
	// auto &children = self.Children<ListParseResult>();
	// auto &use_target = children.Child<ListParseResult>(1);
	// TODO Should be changed below this
	// auto name_expr = make_uniq<ConstantExpression>(Value("foo"));
	// return self.Make<SetStatement>("schema", std::move(name_expr), SetScope::AUTOMATIC);
// }


} // namespace duckdb