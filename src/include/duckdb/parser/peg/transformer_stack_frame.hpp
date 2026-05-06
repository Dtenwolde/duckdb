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
	return std::move(result->Cast<TypedTransformResult<T>>().value);
}

template <typename T>
vector<T> CastRepeatResult(unique_ptr<TransformResultValue> result) {
	auto &repeat = result->Cast<RepeatTransformResultValue>();
	vector<T> out;
	out.reserve(repeat.values.size());
	for (auto &item : repeat.values) {
		out.push_back(CastResult<T>(std::move(item)));
	}
	return out;
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
	void SetParentResult(unique_ptr<TransformResultValue> result) {
		D_ASSERT(parent_frame);
		auto &slot = parent_frame->child_results[parse_result->name];
		if (slot) {
			slot->Cast<RepeatTransformResultValue>().values.push_back(std::move(result));
		} else {
			slot = std::move(result);
		}
	}

	ParseResult *parse_result;
	optional_ptr<TransformerStackFrame> parent_frame;
	InsertionOrderPreservingMap<unique_ptr<TransformResultValue>> child_results; // null = first visit, non-null = re-entry
	TransformState state = INITIALIZING;
};

}