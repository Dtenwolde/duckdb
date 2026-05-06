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


enum TransformState { INITIALIZING, WAITING };

// Anonymous stack frame
// Add stack frame print for crashes
struct TransformerStackFrame {
	// Root frame: no parent
	explicit TransformerStackFrame(ParseResult &parse_result_p) : parse_result(&parse_result_p), parent_frame(nullptr) {
	}
	// Child frame: has parent
	TransformerStackFrame(ParseResult &parse_result_p, TransformerStackFrame &parent_frame_p)
	    : parse_result(&parse_result_p), parent_frame(&parent_frame_p) {
	}
	vector<unique_ptr<TransformResultValue>> GetChoiceResult(ChoiceParseResult &choice) {
		return std::move(child_results[choice.GetResult().name]);
	}

	template <typename T>
	T GetOptionalResult(const string &key, T default_value) {
		auto it = child_results.find(key);
		if (it == child_results.end()) {
			return std::move(default_value);
		}
		D_ASSERT(it->second.size() == 1);
		return CastResult<T>(std::move(it->second[0]));
	}

	void SetParentResult(unique_ptr<TransformResultValue> result) {
		D_ASSERT(parent_frame);
		auto &slot = parent_frame->child_results[parse_result->name];
		slot.push_back(std::move(result));
	}

	ParseResult *parse_result;
	optional_ptr<TransformerStackFrame> parent_frame;
	InsertionOrderPreservingMap<vector<unique_ptr<TransformResultValue>>>
	    child_results;
	TransformState state = INITIALIZING;
};

} // namespace duckdb
