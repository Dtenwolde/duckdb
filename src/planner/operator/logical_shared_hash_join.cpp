#include "duckdb/planner/operator/logical_shared_hash_join.hpp"

#include <utility>

namespace duckdb {

LogicalSharedHashJoin::LogicalSharedHashJoin(JoinType join_type, string lhs_table_name, LogicalOperator* rhs)
    : LogicalOperator(LogicalOperatorType::LOGICAL_SHARED_HASH_JOIN), lhs_table_name(move(lhs_table_name)), join_type(join_type)
{
	AddChild(move(rhs->children[0]));
	AddChild(move(rhs->children[1]));
}

} // namespace duckdb
