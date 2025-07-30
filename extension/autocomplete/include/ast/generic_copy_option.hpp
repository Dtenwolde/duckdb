#pragma once

#include "duckdb/common/string.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

struct GenericCopyOption {
	string name;
	Value value = Value(true); // Default value
};

} // namespace duckdb
