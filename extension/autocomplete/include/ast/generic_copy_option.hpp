#pragma once

#include "duckdb/common/string.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

struct GenericCopyOption {
	string name;
	vector<Value> children; // Default value
};

} // namespace duckdb
