#include "duckdb/optimizer/shared_hash_join.h"
#include "duckdb/planner/operator/logical_limit.hpp"

namespace duckdb {

SharedHashJoin::SharedHashJoin(ClientContext &context) : context(context) {
}

void SharedHashJoin::Optimize(LogicalOperator *op) {
	if (op->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
		auto left_child = (LogicalOperator*) op->children[0].get();
		auto right_child = (LogicalOperator*) op->children[1].get();
		if (left_child->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
			Optimize(left_child);
		} else {
			if (left_child->type == LogicalOperatorType::LOGICAL_GET) {
				context.SharedTable(((LogicalGet *)left_child)->table_name);
			}
		}
		if (right_child->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
			Optimize(right_child);
		} else {
			if (right_child->type == LogicalOperatorType::LOGICAL_GET) {
				context.SharedTable(((LogicalGet *)left_child)->table_name);
			}
		}
	} else {
		for (auto &child : op->children) {
			Optimize(child.get());
		}
	}
}

} // namespace duckdb