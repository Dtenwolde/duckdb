#pragma once

#include "duckdb/common/constants.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {
class LogicalOperator;
class Optimizer;

class SharedHashJoin {

public:
	SharedHashJoin(duckdb::ClientContext &context);
	//! Optimize joins on same hash table
	unique_ptr<LogicalOperator> Optimize(unique_ptr<LogicalOperator> op);

private:
	ClientContext &context;
};

} // namespace duckdb
