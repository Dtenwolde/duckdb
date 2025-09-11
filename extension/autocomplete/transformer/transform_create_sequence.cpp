#include "ast/sequence_option.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<CreateStatement> PEGTransformerFactory::TransformCreateSequenceStmt(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto if_not_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto qualified_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(2));
	auto result = make_uniq<CreateStatement>();
	auto info = make_uniq<CreateSequenceInfo>();
	info->on_conflict = if_not_exists ? OnCreateConflict::IGNORE_ON_CONFLICT : OnCreateConflict::ERROR_ON_CONFLICT;

	auto opt_sequence_options = list_pr.Child<OptionalParseResult>(3);
	case_insensitive_map_t<SequenceOption> sequence_options;
	if (opt_sequence_options.HasResult()) {
		auto repeat_seq_options = opt_sequence_options.optional_result->Cast<RepeatParseResult>();
		for (auto seq_option : repeat_seq_options.children) {
			auto seq_result = transformer.Transform<pair<string, SequenceOption>>(seq_option);
			if (sequence_options.find(seq_result.first) != sequence_options.end()) {
				throw ParserException("CREATE SEQUENCE option %s has already been specified before.", seq_result.first);
			}
			sequence_options.insert(seq_result);
		}
	}
	bool no_min = sequence_options.find("nominvalue") != sequence_options.end();
	bool no_max = sequence_options.find("nomaxvalue") != sequence_options.end();
	int64_t default_start_value = info->start_value;
	bool has_start_value = false;

	for (auto option : sequence_options) {
		if (option.first == "increment") {
			auto seq_val_option = reinterpret_cast<ValueSequenceOption &>(option.second);
			info->increment = seq_val_option.value.GetValue<int64_t>();
			if (info->increment == 0) {
				throw ParserException("Increment must not be zero");
			} else if (info->increment < 0) {
				default_start_value = info->max_value = -1;
				info->min_value = NumericLimits<int64_t>::Minimum();
			} else {
				default_start_value = info->min_value = 0;
				info->max_value = NumericLimits<int64_t>::Maximum();
			}
		} else if (option.first == "minvalue") {
			if (no_min) {
				continue;
			}
			auto seq_val_option = reinterpret_cast<ValueSequenceOption &>(option.second);
			info->min_value = seq_val_option.value.GetValue<int64_t>();
			if (info->increment > 0) {
				default_start_value = info->min_value;
			}
		} else if (option.first == "maxvalue") {
			if (no_max) {
				continue;
			}
			auto seq_val_option = reinterpret_cast<ValueSequenceOption &>(option.second);
			info->max_value = seq_val_option.value.GetValue<int64_t>();
			if (info->increment < 0) {
				default_start_value = info->max_value;
			}
		} else if (option.first == "start") {
			auto seq_val_option = reinterpret_cast<ValueSequenceOption &>(option.second);
			info->start_value = seq_val_option.value.GetValue<int64_t>();
			has_start_value = true;
		} else if (option.first == "cycle") {
			auto seq_val_option = reinterpret_cast<ValueSequenceOption &>(option.second);
			info->cycle = seq_val_option.value.GetValue<bool>();
		} else {
			throw ParserException("Unrecognized option \"%s\" for CREATE SEQUENCE", option.first);
		}
	}
	if (!has_start_value) {
		info->start_value = default_start_value;
	}
	if (info->max_value <= info->min_value) {
		throw ParserException("MINVALUE (%lld) must be less than MAXVALUE (%lld)", info->min_value, info->max_value);
	}
	if (info->start_value < info->min_value) {
		throw ParserException("START value (%lld) cannot be less than MINVALUE (%lld)", info->start_value,
							  info->min_value);
	}
	if (info->start_value > info->max_value) {
		throw ParserException("START value (%lld) cannot be greater than MAXVALUE (%lld)", info->start_value,
							  info->max_value);
	}
	result->info = std::move(info);
	return result;
}

pair<string, SequenceOption> PEGTransformerFactory::TransformSequenceOption(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<pair<string, SequenceOption>>(list_pr.Child<ChoiceParseResult>(0).result);
}

pair<string, SequenceOption> PEGTransformerFactory::TransformSeqSetCycle(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	if (list_pr.Child<OptionalParseResult>(0).HasResult()) {
		return make_pair("cycle", ValueSequenceOption(SequenceInfo::SEQ_CYCLE, false));
	}
	return make_pair("cycle", ValueSequenceOption(SequenceInfo::SEQ_CYCLE, true));
}


pair<string, SequenceOption> PEGTransformerFactory::TransformSeqSetIncrement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2));
	if (expr->GetExpressionClass() != ExpressionClass::CONSTANT) {
		throw ParserException("Expected constant expression.");
	}
	auto const_expr = expr->Cast<ConstantExpression>();
	return make_pair("increment", ValueSequenceOption(SequenceInfo::SEQ_INC, const_expr.value));
}


pair<string, SequenceOption> PEGTransformerFactory::TransformSeqSetMinMax(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1));
	auto rule_name = transformer.TransformEnum<string>(list_pr.Child<ListParseResult>(0));

	if (expr->GetExpressionClass() != ExpressionClass::CONSTANT) {
		throw ParserException("Expected constant expression.");
	}
	auto const_expr = expr->Cast<ConstantExpression>();
	auto seq_info = rule_name == "minvalue" ? SequenceInfo::SEQ_MIN : SequenceInfo::SEQ_MAX;
	return make_pair(rule_name, ValueSequenceOption(seq_info, const_expr.value));
}


pair<string, SequenceOption> PEGTransformerFactory::TransformNoMinMax(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto rule_name = transformer.TransformEnum<string>(list_pr.Child<ListParseResult>(1));
	auto seq_info = rule_name == "minvalue" ? SequenceInfo::SEQ_MIN : SequenceInfo::SEQ_MAX;
	return make_pair("no" + rule_name, ValueSequenceOption(seq_info, true));
}


pair<string, SequenceOption> PEGTransformerFactory::TransformSeqStartWith(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2));

	if (expr->GetExpressionClass() != ExpressionClass::CONSTANT) {
		throw ParserException("Expected constant expression.");
	}
	auto const_expr = expr->Cast<ConstantExpression>();
	return make_pair("start", ValueSequenceOption(SequenceInfo::SEQ_START, const_expr.value));
}

pair<string, SequenceOption> PEGTransformerFactory::TransformSeqOwnedBy(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// Unused by old transformer
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto qualified_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(2));
	return make_pair("owned", QualifiedSequenceOption(SequenceInfo::SEQ_OWN, qualified_name));
}

} // namespace duckdb
