#include "duckdb/parser/peg/matcher_first_set.hpp"

#include "duckdb/common/queue.hpp"
#include "duckdb/common/reference_map.hpp"
#include "duckdb/parser/peg/matcher/first_choice_matcher.hpp"
#include "duckdb/parser/peg/matcher/list_matcher.hpp"
#include "duckdb/parser/peg/matcher/optional_matcher.hpp"
#include "duckdb/parser/peg/matcher/repeat_matcher.hpp"

#include <algorithm>

namespace duckdb {

MatcherFirstSet::MatcherFirstSet(vector<uint32_t> literals_p, bool nullable_p)
    : literals(std::move(literals_p)), unknown(false), nullable(nullable_p) {
	for (auto literal : literals) {
		if (!literal || literal > LiteralInfo::MAX_LITERAL_ID) {
			*this = MatcherFirstSet();
			return;
		}
	}
	std::sort(literals.begin(), literals.end());
	literals.erase(std::unique(literals.begin(), literals.end()), literals.end());
}

bool MatcherFirstSet::CanStartWith(uint32_t literal) const {
	return unknown || nullable || std::binary_search(literals.begin(), literals.end(), literal);
}

void MatcherFirstSet::Merge(const MatcherFirstSet &other) {
	if (unknown) {
		return;
	}
	if (other.unknown) {
		*this = MatcherFirstSet();
		return;
	}
	nullable |= other.nullable;
	literals.insert(literals.end(), other.literals.begin(), other.literals.end());
	std::sort(literals.begin(), literals.end());
	literals.erase(std::unique(literals.begin(), literals.end()), literals.end());
}

bool MatcherFirstSet::operator==(const MatcherFirstSet &other) const {
	return unknown == other.unknown && nullable == other.nullable && literals == other.literals;
}

class MatcherFirstSetBuilder {
private:
	struct Node {
		explicit Node(const Matcher &matcher) : matcher(matcher) {
		}

		const Matcher &matcher;
		vector<idx_t> children;
		vector<idx_t> parents;
		MatcherFirstSet first_set;
		bool builtin = false;
		bool queued = false;
	};

public:
	MatcherFirstSetBuilder(const vector<reference<Matcher>> &builtins, const GrammarLiteralTable &table)
	    : table(table) {
		for (auto &matcher : builtins) {
			auto index = AddMatcher(matcher.get());
			nodes[index].builtin = true;
		}
		// Discover edges iteratively; recursive grammar references must not recurse on the C++ stack.
		for (idx_t index = 0; index < nodes.size(); index++) {
			if (!nodes[index].builtin) {
				continue;
			}
			auto &matcher = nodes[index].matcher;
			switch (matcher.Type()) {
			case MatcherType::LIST:
				for (auto &child : matcher.Cast<ListMatcher>().matchers) {
					AddChild(index, child.get());
				}
				break;
			case MatcherType::CHOICE:
				for (auto &child : matcher.Cast<ChoiceMatcher>().matchers) {
					AddChild(index, child.get());
				}
				break;
			case MatcherType::OPTIONAL:
				AddChild(index, matcher.Cast<OptionalMatcher>().GetChildMatcher());
				break;
			case MatcherType::REPEAT:
				AddChild(index, matcher.Cast<RepeatMatcher>().GetChildMatcher());
				break;
			default:
				throw InternalException("Unexpected built-in matcher in FIRST analysis");
			}
		}
		for (idx_t index = 0; index < nodes.size(); index++) {
			Schedule(index);
		}
		// Start at unknown and refine only proven information, including through recursive references.
		while (!pending.empty()) {
			auto index = pending.front();
			pending.pop();
			auto &node = nodes[index];
			node.queued = false;
			auto first_set = Compute(node);
			if (first_set == node.first_set) {
				continue;
			}
			node.first_set = std::move(first_set);
			for (auto parent : node.parents) {
				Schedule(parent);
			}
		}
	}

	const MatcherFirstSet &Get(const Matcher &matcher) const {
		return nodes[indices.at(matcher)].first_set;
	}

private:
	idx_t AddMatcher(const Matcher &matcher) {
		auto entry = indices.find(matcher);
		if (entry != indices.end()) {
			return entry->second;
		}
		auto index = nodes.size();
		indices.emplace(matcher, index);
		nodes.emplace_back(matcher);
		return index;
	}

	void AddChild(idx_t parent, const Matcher &matcher) {
		auto child = AddMatcher(matcher);
		nodes[parent].children.push_back(child);
		nodes[child].parents.push_back(parent);
	}

	void Schedule(idx_t index) {
		if (!nodes[index].queued) {
			nodes[index].queued = true;
			pending.push(index);
		}
	}

	MatcherFirstSet Compute(const Node &node) const {
		if (!node.builtin) {
			return node.matcher.GetFirstSet(table);
		}
		switch (node.matcher.Type()) {
		case MatcherType::LIST: {
			MatcherFirstSet result({}, true);
			for (auto child : node.children) {
				if (result.unknown || !result.nullable) {
					break;
				}
				result.nullable = false;
				result.Merge(nodes[child].first_set);
			}
			return result;
		}
		case MatcherType::CHOICE: {
			MatcherFirstSet result({}, false);
			for (auto child : node.children) {
				result.Merge(nodes[child].first_set);
				if (result.unknown) {
					break;
				}
			}
			return result;
		}
		case MatcherType::OPTIONAL: {
			auto result = nodes[node.children[0]].first_set;
			result.nullable = true;
			return result;
		}
		case MatcherType::REPEAT:
			return nodes[node.children[0]].first_set;
		default:
			throw InternalException("Unexpected built-in matcher in FIRST analysis");
		}
	}

private:
	const GrammarLiteralTable &table;
	reference_map_t<const Matcher, idx_t> indices;
	vector<Node> nodes;
	queue<idx_t> pending;
};

void InitializeMatcherFirstSets(const vector<reference<Matcher>> &builtins,
                                const vector<reference<FirstChoiceMatcher>> &choices,
                                const GrammarLiteralTable &table) {
	MatcherFirstSetBuilder builder(builtins, table);
	for (auto &choice_ref : choices) {
		auto &choice = choice_ref.get();
		vector<MatcherFirstSet> sets;
		sets.reserve(choice.matchers.size());
		for (auto &child : choice.matchers) {
			sets.push_back(builder.Get(child.get()));
		}
		choice.SetFirstSets(std::move(sets));
	}
}

void FirstChoiceMatcher::SetFirstSets(vector<MatcherFirstSet> sets) {
	D_ASSERT(sets.size() == matchers.size());
	for (auto &set : sets) {
		if (!set.IsUnknown() && !set.CanMatchEmpty()) {
			first_sets = std::move(sets);
			return;
		}
	}
	first_sets.clear();
}

idx_t FirstChoiceMatcher::NextChild(MatchState &state, idx_t child_index) const {
	D_ASSERT(first_sets.size() == matchers.size());
	if (child_index >= first_sets.size() || first_sets[child_index].IsUnknown() ||
	    first_sets[child_index].CanMatchEmpty()) {
		return child_index;
	}
	auto token = state.token_iterator.Current();
	if (!token || token->type == TokenType::END_OF_INPUT_AUTOCOMPLETE) {
		return child_index;
	}
	auto literal = state.token_iterator.CurrentLiteralInfo(table).LiteralId();
	while (child_index < first_sets.size() && !first_sets[child_index].CanStartWith(literal)) {
		child_index++;
	}
	return child_index;
}

} // namespace duckdb
