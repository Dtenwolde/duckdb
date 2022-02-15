#pragma once

namespace duckdb {

class SharedJoinTable {
public:
	explicit SharedJoinTable(bool found);
	bool found;
};

} // namespace duckdb