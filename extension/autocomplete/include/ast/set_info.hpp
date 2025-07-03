#pragma once

#include "duckdb/common/enums/set_scope.hpp"
#include "duckdb/common/string.hpp"

namespace duckdb {

// This struct represents the semantic value of a parsed SETTING clause.
// It lives in its own header because it's part of the public "contract"
// of the parser. Other parts of the system will use this struct to
// understand the result of the transformation.
struct SettingInfo {
	string name;
	SetScope scope = SetScope::AUTOMATIC; // Default value is defined here
};

} // namespace duckdb