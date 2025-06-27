#pragma once

#include "duckdb/common/vector.hpp"
#include "duckdb/common/unique_ptr.hpp"

namespace duckdb {
class SQLStatement;
class ClientContext;

//! The ParserOverride is an extension point that allows installing a custom parser
class ParserOverride {
public:
	virtual ~ParserOverride() = default;

	//! Tries to parse a query.
	//! If it succeeds, it returns a vector of SQL statements.
	//! If it fails to parse because the syntax is not supported by this override,
	//! it should return an empty vector, allowing the system to fall back to the default parser.
	//! If it encounters a syntax error within a structure it *does* recognize, it should throw an exception.
	virtual vector<unique_ptr<SQLStatement>> Parse(ClientContext &context, const string &query) = 0;
};

} // namespace duckdb