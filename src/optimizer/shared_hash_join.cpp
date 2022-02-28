#include "duckdb/optimizer/shared_hash_join.hpp"

#include "duckdb/planner/operator/logical_limit.hpp"
#include "duckdb/planner/operator/logical_shared_hash_join.hpp"

#include <iostream>

namespace duckdb {

SharedHashJoin::SharedHashJoin(ClientContext &context) : context(context) {
}

unique_ptr<LogicalOperator> SharedHashJoin::Optimize(LogicalOperator *op) {
	if (op->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
		auto left_child = (LogicalOperator *)op->children[0].get();
		auto right_child = (LogicalOperator *)op->children[1].get();
		if (left_child->type == LogicalOperatorType::LOGICAL_GET &&
		    right_child->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
			auto shared_hash_join = make_unique<LogicalSharedHashJoin>(((LogicalJoin &)*op).join_type,
			                                                           ((LogicalGet *)left_child)->table_name,
			                                                           (LogicalComparisonJoin*)right_child);
			op->children.erase(op->children.begin() + 1);
			op->AddChild(move(shared_hash_join));
		}
	}
	for (auto &child : op->children) {
		Optimize(child.get());
	}

	return op;
}
//		for (auto &child : op->children) {
//			auto child_converted = (LogicalOperator*) child.get();
//			if (child_converted->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
//				std::cout << "Found Comparison Join" << std::endl;
//				child_converted = Optimize(child_converted);
//				std::cout << "Done optimize child converted" << std::endl;
//			} else {
//				if (child_converted->type == LogicalOperatorType::LOGICAL_GET) {
//					std::cout << "Found get" << std::endl;
//
//				}
//			}
//
//		}
//		auto left_child = (LogicalOperator*) op->children[0].get();
//		auto right_child = (LogicalOperator*) op->children[1].get();
//		if (left_child->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
//			Optimize(left_child);
//		} else {
//			if (left_child->type == LogicalOperatorType::LOGICAL_GET) {
//				bool is_duplicate = context.FindDuplicateTables(((LogicalGet *)left_child));
//				if (is_duplicate) {
//					auto shared_hash_join = make_unique<LogicalSharedHashJoin>(((LogicalJoin &)*op).join_type,
//					                                                           ((LogicalGet *)left_child)->table_name,
//					                                                           unique_ptr<LogicalOperator>(right_child));
//					std::cout << "Duplicate found" << std::endl;
//					return move(shared_hash_join.get());
//				}
//			}
//		}
//		if (right_child->type == LogicalOperatorType::LOGICAL_COMPARISON_JOIN) {
//			Optimize(right_child);
//		} else {
//			if (right_child->type == LogicalOperatorType::LOGICAL_GET) {
//				bool is_duplicate = context.FindDuplicateTables(((LogicalGet *)right_child));
//				if (is_duplicate) {
//					auto shared_hash_join = make_unique<LogicalSharedHashJoin>(((LogicalJoin &)*op).join_type,
//					                                              ((LogicalGet *)right_child)->table_name,
//					                                              unique_ptr<LogicalOperator>(left_child));
//					std::cout << "Duplicate found" << std::endl;
//					return move((LogicalOperator*) shared_hash_join.get());
//				}
//			}
//		}
//	else if (op->type == LogicalOperatorType::LOGICAL_SHARED_HASH_JOIN) {
//		std::cout << "Further optimization found" << std::endl;
//	}


//	return op;

} // namespace duckdb