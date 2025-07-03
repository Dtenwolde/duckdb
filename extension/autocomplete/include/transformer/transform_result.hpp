#pragma once

#include "duckdb/common/unique_ptr.hpp"

namespace duckdb {

// 1. The abstract base class
struct TransformResultValue {
	virtual ~TransformResultValue() = default;
};

// 2. The templated wrapper that holds the actual value
template<class T>
struct TypedTransformResult : public TransformResultValue {
	explicit TypedTransformResult(T value_p) : value(std::move(value_p)) {}
	T value;
};

} // namespace duckdb