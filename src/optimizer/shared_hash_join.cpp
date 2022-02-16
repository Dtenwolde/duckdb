#include "duckdb/optimizer/shared_hash_join.h"
#include "duckdb/planner/operator/logical_limit.hpp"

namespace duckdb {

SharedHashJoin::SharedHashJoin(ClientContext &context) : context(context) {
}

unique_ptr<LogicalOperator> SharedHashJoin::Optimize(unique_ptr<LogicalOperator> op) {
	if (op->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
		const auto &join = (LogicalComparisonJoin &)*op;
		auto &left_child = op->children[0];
		auto &right_child = op->children[1];
		if (left_child->type == LogicalOperatorType::LOGICAL_GET) {
			context.SharedTable(((LogicalGet *)left_child.get())->table_name);
		}
		if (right_child->type == LogicalOperatorType::LOGICAL_GET) {
			context.SharedTable(((LogicalGet *)left_child.get())->table_name);
		}

	} else {
		for (auto &child : op->children) {
			child = Optimize(move(child));
		}
	}
	return op;
}

} // namespace duckdb