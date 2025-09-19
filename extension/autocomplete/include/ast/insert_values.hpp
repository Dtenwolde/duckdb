#pragma once
#include "duckdb/parser/sql_statement.hpp"

namespace duckdb {
struct InsertValues {
	bool default_values = false;
	unique_ptr<SelectStatement> select_statement;
};


} // namespace duckdb