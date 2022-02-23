#pragma once

#include "duckdb/common/constants.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {
class LogicalOperator;
class Optimizer;

class SharedHashJoin {

public:
	explicit SharedHashJoin(duckdb::ClientContext &context);
	//! Optimize joins on same hash table
	void Optimize(LogicalOperator* op);

private:
	ClientContext &context;
};

} // namespace duckdb
