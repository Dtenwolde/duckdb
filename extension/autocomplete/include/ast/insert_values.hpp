#pragma once
#include "duckdb/parser/sql_statement.hpp"

namespace duckdb {
struct InsertValues {
	bool default_values = false;
	unique_ptr<SQLStatement> sql_statement;
};


} // namespace duckdb