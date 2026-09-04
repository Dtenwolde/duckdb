//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg/matcher_stack.hpp
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/array.hpp"
#include "duckdb/common/optional.hpp"
#include "duckdb/common/optional_idx.hpp"
#include "duckdb/parser/peg/matcher.hpp"
#include "duckdb/storage/arena_allocator.hpp"

namespace duckdb {

using match_frame_index_t = idx_t;

class MatchStack;

enum class MatchFrameState : uint8_t { INITIALIZE, EXECUTE };
enum class MatchResultState : uint8_t { NONE, FAILURE, SUCCESS };

struct PackratMatchState {
	static bool IsEnabled(const Matcher &matcher, const MatchState &state) {
		return state.context.packrat_cache && matcher.IsPackratMemoized() && matcher.GetPackratId().IsValid();
	}

	optional<MatcherResult> TryLoadCachedResult(const Matcher &matcher, MatchState &state);
	void StoreResult(const Matcher &matcher, MatchState &state, const MatcherResult &result) const;

private:
	optional_idx token_index_before;
	idx_t max_token_index_before = 0;
};

struct MatchStackFrame {
	MatchStackFrame(match_frame_index_t frame_index, const Matcher &matcher, MatchState &state);
	virtual ~MatchStackFrame() = default;

	virtual void Execute(MatchStack &stack) = 0;
	void SetResult(const MatcherResult &result);
	bool HasResult() const;
	MatcherResult GetResult() const;
	void SetChildResult(const MatcherResult &result);
	bool HasChildResult() const;
	MatcherResult TakeChildResult();

	const match_frame_index_t frame_index;
	const Matcher &matcher;
	MatchState &match_state;
	MatchFrameState state = MatchFrameState::INITIALIZE;
	MatchResultState result_state = MatchResultState::NONE;
	optional_ptr<ParseResult> parse_result;
	MatchResultState child_result_state = MatchResultState::NONE;
	optional_ptr<ParseResult> child_parse_result;
	PackratMatchState packrat_state;
};

class MatchStack {
public:
	MatchStack();
	~MatchStack();

	MatcherResult Execute(const Matcher &matcher, MatchState &state);
	void PushChildFrame(MatchStackFrame &parent, const Matcher &matcher, MatchState &state);

private:
	static constexpr idx_t FRAME_SEGMENT_CAPACITY = 32;
	static constexpr idx_t INLINE_FRAME_SEGMENT_COUNT = 2;

	static bool IsTerminalMatcher(const Matcher &matcher);
	static idx_t FrameSlotSize();
	static idx_t FrameSegmentSize();
	MatcherResult ExecuteTerminalMatcher(const Matcher &matcher, MatchState &state);
	void AllocateFrameSegment();
	data_ptr_t GetFrameSegment(idx_t segment_index) const;
	void SetActiveFrameSegment(idx_t segment_index);
	data_ptr_t AllocateFrameSlot();
	data_ptr_t AllocateFrameSlot(idx_t size);
	void DestroyTopFrame();
	void PushFrame(const Matcher &matcher, MatchState &state);
	template <class FRAME, class MATCHER>
	void PushFrameInternal(match_frame_index_t frame_index, const MATCHER &matcher, MatchState &state) {
		static_assert(alignof(FRAME) <= alignof(idx_t), "Matcher frame alignment is too large");
		if (frames.size() == frames.capacity()) {
			frames.reserve(frames.size() + FRAME_SEGMENT_CAPACITY);
		}
		auto frame_slot = AllocateFrameSlot(sizeof(FRAME));
		frames.push_back(new (frame_slot) FRAME(frame_index, matcher, state));
	}
	void InitializeFrame(MatchStackFrame &frame);
	void ExecuteFrame(MatchStackFrame &frame);
	MatcherResult FinalizeFrame(MatchStackFrame &frame);
	MatcherResult ExecuteInternal(const Matcher &matcher, MatchState &state);

private:
	ArenaAllocator frame_allocator;
	array<data_ptr_t, INLINE_FRAME_SEGMENT_COUNT> inline_frame_segments {};
	vector<data_ptr_t> overflow_frame_segments;
	idx_t frame_segment_count = 0;
	data_ptr_t active_frame_segment = nullptr;
	idx_t active_frame_segment_index = DConstants::INVALID_INDEX;
	vector<MatchStackFrame *> frames;
};

} // namespace duckdb
