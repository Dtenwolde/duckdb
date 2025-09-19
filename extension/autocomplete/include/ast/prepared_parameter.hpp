#pragma once
#include "duckdb/parser/expression/parameter_expression.hpp"

namespace duckdb {
struct PreparedParameter {
	PreparedParamType type;
	string identifier;
	idx_t number;
};

}