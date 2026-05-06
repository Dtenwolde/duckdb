#pragma once

#include "duckdb/common/common.hpp"

namespace duckdb {

struct TransformResultValue {
	virtual ~TransformResultValue() = default;

	template <class TARGET>
	TARGET &Cast() {
		auto *result = dynamic_cast<TARGET *>(this);
		if (!result) {
			throw InternalException("Failed to cast TransformResultValue to %s", typeid(TARGET).name());
		}
		return *result;
	}
};

template <class T>
struct TypedTransformResult : public TransformResultValue {
	explicit TypedTransformResult(T value_p) : value(std::move(value_p)) {
	}
	T value;
};

struct RepeatTransformResultValue : public TransformResultValue {
	vector<unique_ptr<TransformResultValue>> values;
};


} // namespace duckdb
