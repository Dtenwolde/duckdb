#include "duckdb/optimizer/shared_hash_join.h"
#include "duckdb/planner/operator/logical_order.hpp"
#include "duckdb/planner/operator/logical_limit.hpp"

namespace duckdb {

SharedHashJoin::SharedHashJoin(ClientContext &context) : context(context) {
}

unique_ptr<LogicalOperator> SharedHashJoin::Optimize(unique_ptr<LogicalOperator> op) {
	if (op->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
		const auto &join = (LogicalComparisonJoin &)*op;

	} else {
		for (auto &child : op->children) {
			child = Optimize(move(child));
		}
	}
	return op;
}

} // namespace duckdb