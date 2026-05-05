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

enum TransformState {
	INITIALIZING,
	WAITING
};

struct TransformerStackFrame {
	// Root frame: no parent
	explicit TransformerStackFrame(ParseResult &parse_result_p)
	    : parse_result(&parse_result_p), parent_frame(nullptr) {
	}
	// Child frame: has parent
	TransformerStackFrame(ParseResult &parse_result_p, TransformerStackFrame &parent_frame_p)
	    : parse_result(&parse_result_p), parent_frame(&parent_frame_p) {
	}
	ParseResult *parse_result;
	optional_ptr<TransformerStackFrame> parent_frame;
	InsertionOrderPreservingMap<unique_ptr<TransformResultValue>> child_results; // null = first visit, non-null = re-entry
	TransformState state = INITIALIZING;
};

}