#pragma once
#include "duckdb/parser/parsed_expression.hpp"

namespace duckdb {

struct UpdateSetElement {
	string column_name;
	unique_ptr<ParsedExpression> expression; // Default value is defined here
};
}