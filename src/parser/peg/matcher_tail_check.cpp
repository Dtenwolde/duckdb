#include "duckdb/parser/peg/matcher.hpp"
#include "duckdb/parser/peg/matcher/list.hpp"
#include "duckdb/common/reference_map.hpp"

namespace duckdb {

//===--------------------------------------------------------------------===//
// Tail check analysis
//===--------------------------------------------------------------------===//
namespace {

enum class TailBracket : uint8_t { NONE, OPEN, CLOSE };

//! Calls fun(child) for every child of a built-in composite matcher
template <class FUN>
static void ForEachChildMatcher(Matcher &matcher, FUN &&fun) {
	switch (matcher.Type()) {
	case MatcherType::LIST:
		for (auto &child : matcher.Cast<ListMatcher>().matchers) {
			fun(child.get());
		}
		break;
	case MatcherType::CHOICE:
		for (auto &child : matcher.Cast<ChoiceMatcher>().matchers) {
			fun(child.get());
		}
		break;
	case MatcherType::OPTIONAL:
		fun(matcher.Cast<OptionalMatcher>().GetChildMatcher());
		break;
	case MatcherType::REPEAT:
		fun(matcher.Cast<RepeatMatcher>().GetChildMatcher());
		break;
	default:
		break;
	}
}

//! Derived matchers can override StartMatch, so only matchers marked as structural follow the semantics of their type
static bool IsCompositeMatcher(const Matcher &matcher) {
	if (!matcher.IsStructural()) {
		return false;
	}
	auto type = matcher.Type();
	return type == MatcherType::LIST || type == MatcherType::CHOICE || type == MatcherType::OPTIONAL ||
	       type == MatcherType::REPEAT;
}

//! Tokens a matcher consumes outside of balanced bracket pairs; unsafe when they cannot be determined
struct ConsumedTokens {
	MatcherTokenSet tokens;
	bool safe = true;

	bool Merge(const ConsumedTokens &other) {
		bool changed = tokens.MergeTokens(other.tokens);
		if (safe && !other.safe) {
			safe = false;
			changed = true;
		}
		return changed;
	}
	bool MarkUnsafe() {
		if (!safe) {
			return false;
		}
		safe = false;
		return true;
	}
};

template <class FUN>
static void ForEachLiteralId(const MatcherTokenSet &set, FUN &&fun) {
	for (idx_t word = 0; word < set.literals.size(); word++) {
		auto bits = set.literals[word];
		while (bits) {
			auto bit = static_cast<idx_t>(__builtin_ctzll(bits));
			fun(word * 64 + bit);
			bits &= bits - 1;
		}
	}
}

struct TailCheckNode {
	explicit TailCheckNode(Matcher &matcher_p) : matcher(matcher_p) {
	}

	Matcher &matcher;
	//! child nodes of a composite matcher, in order
	vector<idx_t> children;
	vector<idx_t> parents;
	ConsumedTokens consumed;
	//! set when the matcher consumes exactly one bracket literal
	TailBracket bracket = TailBracket::NONE;
	idx_t bracket_pair = 0;
	bool queued = false;
};

class TailCheckAnalysis {
public:
	TailCheckAnalysis(const vector<unique_ptr<Matcher>> &all_matchers, const GrammarLiteralTable &table);

	//! The tail check of every sequence that has one
	vector<pair<reference<ListMatcher>, unique_ptr<MatcherTailCheck>>> CreateTailChecks();

private:
	static constexpr idx_t BRACKET_PAIRS = 3;

	TailBracket BracketLiteral(idx_t literal_id, idx_t &pair) const;
	//! Merges the tokens consumed by children[begin, end) outside of the bracket pairs among them into result
	bool AnalyzeSequence(const vector<idx_t> &children, idx_t begin, idx_t end, ConsumedTokens &result) const;
	bool Update(idx_t node_index);
	//! No token can be consumed by the prefix and start the element
	bool Disjoint(const MatcherTokenSet &alphabet, const MatcherTokenSet &required) const;

private:
	const GrammarLiteralTable &table;
	vector<TailCheckNode> nodes;
	uint16_t open_literals[BRACKET_PAIRS];
	uint16_t close_literals[BRACKET_PAIRS];
	//! class of the token spelled like each literal
	vector<uint8_t> literal_classes;
};

TailCheckAnalysis::TailCheckAnalysis(const vector<unique_ptr<Matcher>> &all_matchers,
                                     const GrammarLiteralTable &table_p)
    : table(table_p) {
	static constexpr const char OPEN_BRACKETS[BRACKET_PAIRS] = {'(', '[', '{'};
	static constexpr const char CLOSE_BRACKETS[BRACKET_PAIRS] = {')', ']', '}'};
	for (idx_t pair = 0; pair < BRACKET_PAIRS; pair++) {
		open_literals[pair] = table.Lookup(&OPEN_BRACKETS[pair], 1).LiteralId();
		close_literals[pair] = table.Lookup(&CLOSE_BRACKETS[pair], 1).LiteralId();
	}
	literal_classes.resize(idx_t(LiteralInfo::MAX_LITERAL_ID) + 1, 0);
	table.ForEachLiteralText([&](const string &text, const LiteralInfo &info) {
		uint8_t token_class;
		if (info.IsKeyword()) {
			token_class = MatcherTokenClass::WORD;
		} else if (!text.empty() && Tokenizer::CharacterIsKeyword(text[0])) {
			token_class = MatcherTokenClass::WORD | MatcherTokenClass::IDENTIFIER;
		} else {
			token_class = ComputeMatcherTokenClass(TokenType::OPERATOR, text);
		}
		literal_classes[info.LiteralId()] = token_class;
	});
	// only reachable matchers have FIRST sets
	reference_map_t<const Matcher, idx_t> index;
	idx_t literal_words = 0;
	for (auto &matcher_p : all_matchers) {
		auto &first = matcher_p->first_set;
		if (!first.computed) {
			continue;
		}
		index[*matcher_p] = nodes.size();
		nodes.emplace_back(*matcher_p);
		literal_words = MaxValue(literal_words, MaxValue(first.literals.size(), first.second.literals.size()));
	}
	for (idx_t node_index = 0; node_index < nodes.size(); node_index++) {
		auto &node = nodes[node_index];
		auto &matcher = node.matcher;
		auto &first = matcher.first_set;
		if (IsCompositeMatcher(matcher)) {
			bool complete = true;
			ForEachChildMatcher(matcher, [&](Matcher &child) {
				auto entry = index.find(child);
				if (entry == index.end()) {
					complete = false;
				} else {
					node.children.push_back(entry->second);
				}
			});
			if (!complete) {
				node.children.clear();
				node.consumed.safe = false;
			}
			for (auto child : node.children) {
				nodes[child].parents.push_back(node_index);
			}
			// sized once, so merging never reallocates
			node.consumed.tokens.literals.resize(literal_words, 0);
		} else if (matcher.IsAtomic() && !first.any && !first.one.any && !first.second.any) {
			// built-in atomic matchers consume tokens of their FIRST and SECOND sets only
			node.consumed.tokens.MergeTokens(first);
			node.consumed.tokens.MergeTokens(first.second);
			if (!first.class_mask && first.second.IsEmpty()) {
				idx_t literal_count = 0;
				idx_t only_literal = 0;
				ForEachLiteralId(first, [&](idx_t literal_id) {
					literal_count++;
					only_literal = literal_id;
				});
				if (literal_count == 1) {
					node.bracket = BracketLiteral(only_literal, node.bracket_pair);
				}
			}
			// a bracket is only consumed as part of a balanced pair in one sequence
			if (node.bracket != TailBracket::NONE) {
				node.consumed.safe = false;
			}
		} else {
			node.consumed.safe = false;
		}
	}
}

TailBracket TailCheckAnalysis::BracketLiteral(idx_t literal_id, idx_t &pair) const {
	for (pair = 0; pair < BRACKET_PAIRS; pair++) {
		if (literal_id == open_literals[pair]) {
			return TailBracket::OPEN;
		}
		if (literal_id == close_literals[pair]) {
			return TailBracket::CLOSE;
		}
	}
	return TailBracket::NONE;
}

bool TailCheckAnalysis::AnalyzeSequence(const vector<idx_t> &children, idx_t begin, idx_t end,
                                        ConsumedTokens &result) const {
	bool changed = false;
	idx_t i = begin;
	while (i < end) {
		auto &child = nodes[children[i]];
		if (child.bracket == TailBracket::NONE) {
			changed |= result.Merge(child.consumed);
			i++;
			continue;
		}
		if (child.bracket == TailBracket::CLOSE) {
			return changed | result.MarkUnsafe();
		}
		// find the closing bracket of the same pair, nested pairs included
		idx_t depth = 0;
		idx_t close = i + 1;
		for (; close < end; close++) {
			auto &candidate = nodes[children[close]];
			if (candidate.bracket == TailBracket::NONE || candidate.bracket_pair != child.bracket_pair) {
				continue;
			}
			if (candidate.bracket == TailBracket::OPEN) {
				depth++;
			} else if (depth == 0) {
				break;
			} else {
				depth--;
			}
		}
		if (close >= end) {
			return changed | result.MarkUnsafe();
		}
		// the interior is skipped at runtime, but it has to keep its own brackets balanced
		for (idx_t interior = i + 1; interior < close; interior++) {
			auto &interior_node = nodes[children[interior]];
			if (interior_node.bracket == TailBracket::NONE && !interior_node.consumed.safe) {
				changed |= result.MarkUnsafe();
			}
		}
		i = close + 1;
	}
	return changed;
}

bool TailCheckAnalysis::Update(idx_t node_index) {
	auto &node = nodes[node_index];
	if (node.matcher.Type() == MatcherType::LIST) {
		return AnalyzeSequence(node.children, 0, node.children.size(), node.consumed);
	}
	bool changed = false;
	for (auto child : node.children) {
		changed |= node.consumed.Merge(nodes[child].consumed);
	}
	return changed;
}

bool TailCheckAnalysis::Disjoint(const MatcherTokenSet &alphabet, const MatcherTokenSet &required) const {
	// the classes the tokenizer assigns, so that a class in one set cannot share a token with a class in the other
	static constexpr uint8_t TOKEN_CLASSES[] = {
	    MatcherTokenClass::WORD,     MatcherTokenClass::WORD | MatcherTokenClass::IDENTIFIER,
	    MatcherTokenClass::STRING,   MatcherTokenClass::NUMBER,
	    MatcherTokenClass::OPERATOR, MatcherTokenClass::OPERATOR | MatcherTokenClass::GENERIC_OPERATOR};
	for (auto token_class : TOKEN_CLASSES) {
		if ((token_class & alphabet.class_mask) && (token_class & required.class_mask)) {
			return false;
		}
	}
	bool disjoint = true;
	ForEachLiteralId(required, [&](idx_t literal_id) {
		idx_t pair;
		if (alphabet.HasLiteral(literal_id) || (literal_classes[literal_id] & alphabet.class_mask) ||
		    BracketLiteral(literal_id, pair) != TailBracket::NONE) {
			disjoint = false;
		}
	});
	ForEachLiteralId(alphabet, [&](idx_t literal_id) {
		if (literal_classes[literal_id] & required.class_mask) {
			disjoint = false;
		}
	});
	return disjoint;
}

vector<pair<reference<ListMatcher>, unique_ptr<MatcherTailCheck>>> TailCheckAnalysis::CreateTailChecks() {
	// least fixed point over the recursive grammar: a change re-queues the parents
	vector<idx_t> worklist;
	for (idx_t node_index = 0; node_index < nodes.size(); node_index++) {
		if (IsCompositeMatcher(nodes[node_index].matcher)) {
			nodes[node_index].queued = true;
			worklist.push_back(node_index);
		}
	}
	while (!worklist.empty()) {
		auto node_index = worklist.back();
		worklist.pop_back();
		nodes[node_index].queued = false;
		if (!Update(node_index)) {
			continue;
		}
		for (auto parent : nodes[node_index].parents) {
			if (!nodes[parent].queued) {
				nodes[parent].queued = true;
				worklist.push_back(parent);
			}
		}
	}
	vector<pair<reference<ListMatcher>, unique_ptr<MatcherTailCheck>>> tail_checks;
	for (auto &node : nodes) {
		auto &matcher = node.matcher;
		if (!IsCompositeMatcher(matcher) || matcher.Type() != MatcherType::LIST) {
			continue;
		}
		auto &children = node.children;
		for (idx_t i = 1; i < children.size(); i++) {
			auto &element_first = nodes[children[i]].matcher.first_set;
			if (!element_first.computed || element_first.any || element_first.nullable) {
				continue;
			}
			// a one-token prefix is already covered by the SECOND set
			if (i == 1 && nodes[children[0]].matcher.IsAtomic()) {
				continue;
			}
			ConsumedTokens prefix;
			AnalyzeSequence(children, 0, i, prefix);
			if (!prefix.safe || !Disjoint(prefix.tokens, element_first)) {
				continue;
			}
			auto check = make_uniq<MatcherTailCheck>();
			check->alphabet = std::move(prefix.tokens);
			check->required = static_cast<const MatcherTokenSet &>(element_first);
			tail_checks.emplace_back(matcher.Cast<ListMatcher>(), std::move(check));
			break;
		}
	}
	return tail_checks;
}

} // namespace

void MatcherAllocator::InitializeTailChecks(const GrammarLiteralTable &table) {
	// Clear borrowed links before releasing their owners or making any allocation that can throw.
	for (auto &matcher : matchers) {
		auto &first = matcher->first_set;
		if (first.tail_check_index) {
			first.tail_check_index = 0;
			first.Finalize();
		}
	}
	tail_checks.clear();
	TailCheckAnalysis analysis(matchers, table);
	auto checks = analysis.CreateTailChecks();
	tail_checks.reserve(checks.size());
	for (auto &entry : checks) {
		if (tail_checks.size() >= NumericLimits<uint16_t>::Maximum()) {
			break;
		}
		auto &first = entry.first.get().first_set;
		tail_checks.push_back(std::move(entry.second));
		first.tail_check_index = NumericCast<uint16_t>(tail_checks.size());
		// every token passing the FIRST check goes through MightMatchSecond, which runs the tail check
		first.first_suffices = false;
	}
}

} // namespace duckdb
