#pragma once
#include "transformer/parse_result.hpp"
#include "transformer/transform_result.hpp"

namespace duckdb {

template <typename T>
unique_ptr<TransformResultValue> MakeResult(T value) {
	return make_uniq<TypedTransformResult<T>>(std::move(value));
}

template <typename T>
T CastResult(unique_ptr<TransformResultValue> result) {
	auto *p = dynamic_cast<TypedTransformResult<T> *>(result.get());
	if (!p) {
		throw InternalException("CastResult: unexpected result type");
	}
	return std::move(p->value);
}

struct TransformerStackFrame {
	explicit TransformerStackFrame(ParseResult &parse_result_p) : parse_result(&parse_result_p) {
	}

	ParseResult *parse_result;
	unique_ptr<TransformResultValue> child_result;   // null = first visit, non-null = re-entry
	idx_t step = 0;                                  // multi-child state for left-fold rules; unused by single-child rules
};

}