#pragma once

namespace duckdb {

class LogicalSharedHashJoin : public LogicalOperator {
public:
	explicit LogicalSharedHashJoin(JoinType join_type, string lhs_table_name, LogicalOperator* rhs);

	string lhs_table_name;

	LogicalOperatorType logical_type;

	JoinType join_type;

public:
	vector<ColumnBinding> GetColumnBindings() override {
		return children[0]->GetColumnBindings();
	}

protected:
	void ResolveTypes() override {
		types = children[0]->types;
	}
};



}
