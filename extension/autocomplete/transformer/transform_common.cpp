#include "duckdb/common/extra_type_info.hpp"
#include "duckdb/common/types/decimal.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {
string PEGTransformerFactory::TransformColIdOrString(PEGTransformer &transformer,
													 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return transformer.Transform<string>(choice_pr.result);
}

string PEGTransformerFactory::TransformColId(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	if (choice_pr.result->type == ParseResultType::IDENTIFIER) {
		return choice_pr.result->Cast<IdentifierParseResult>().identifier;
	}
	return transformer.Transform<string>(choice_pr.result);
}

string PEGTransformerFactory::TransformStringLiteral(PEGTransformer &transformer,
													 optional_ptr<ParseResult> parse_result) {
	auto &string_literal_pr = parse_result->Cast<StringLiteralParseResult>();
	return string_literal_pr.result;
}

string PEGTransformerFactory::TransformIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &identifier_pr = list_pr.Child<IdentifierParseResult>(0);
	return identifier_pr.identifier;
}

LogicalType PEGTransformerFactory::TransformType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &type_pr = list_pr.Child<ListParseResult>(0);
	auto type = transformer.Transform<LogicalType>(type_pr.Child<ChoiceParseResult>(0).result);
	auto opt_array_bounds_pr = list_pr.Child<OptionalParseResult>(1);
	if (opt_array_bounds_pr.HasResult()) {
		auto array_bounds_repeat = opt_array_bounds_pr.optional_result->Cast<RepeatParseResult>();
		for (auto &array_bound : array_bounds_repeat.children) {
			auto array_size = transformer.Transform<int64_t>(array_bound);
			if (array_size < 0) {
				type = LogicalType::LIST(type);
			} else if (array_size == 0) {
				// Empty arrays are not supported
				throw ParserException("Arrays must have a size of at least 1");
			} else if (array_size > static_cast<int64_t>(ArrayType::MAX_ARRAY_SIZE)) {
				throw ParserException("Arrays must have a size of at most %d", ArrayType::MAX_ARRAY_SIZE);
			} else {
				type = LogicalType::ARRAY(type, NumericCast<idx_t>(array_size));
			}
		}
	}
	return type;
}

int64_t PEGTransformerFactory::TransformArrayBounds(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<int64_t>(list_pr.Child<ChoiceParseResult>(0).result);
}

int64_t PEGTransformerFactory::TransformArrayKeyword(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// ArrayKeyword <- 'ARRAY'i
	// Empty array so we return -1 to signify it's a list
	return -1;
}

int64_t PEGTransformerFactory::TransformSquareBracketsArray(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto opt_array_size = list_pr.Child<OptionalParseResult>(1);
	if (opt_array_size.HasResult()) {
		return std::stoi(opt_array_size.optional_result->Cast<NumberParseResult>().number);
	}
	return -1;
}

LogicalType PEGTransformerFactory::TransformNumericType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<LogicalType>(list_pr.Child<ChoiceParseResult>(0).result);
}

LogicalType PEGTransformerFactory::TransformSimpleNumericType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.TransformEnum<LogicalType>(list_pr.Child<ChoiceParseResult>(0).result);
}

LogicalType PEGTransformerFactory::TransformDecimalNumericType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<LogicalType>(list_pr.Child<ChoiceParseResult>(0).result);
}

LogicalType PEGTransformerFactory::TransformFloatType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// TODO(Dtenwolde) Unknown if anything should be done with the Parens(Numberliteral)? part.
	return LogicalType(LogicalTypeId::FLOAT);
}

LogicalType PEGTransformerFactory::TransformDecimalType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto opt_type_modifier = list_pr.Child<OptionalParseResult>(1);
	uint8_t width = Decimal::DEFAULT_WIDTH_DECIMAL;
	uint8_t scale = Decimal::DEFAULT_SCALE_DECIMAL;
	if (opt_type_modifier.HasResult()) {
		auto type_modifiers = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(opt_type_modifier.optional_result);
		if (type_modifiers.size() > 2) {
			throw ParserException("A maximum of two modifiers is supported");
		}
		if (type_modifiers[0]->GetExpressionClass() != ExpressionClass::CONSTANT ||
			type_modifiers[1]->GetExpressionClass() != ExpressionClass::CONSTANT) {
			throw ParserException("Type modifiers must be a constant expression");
			}
		// Parsing of width and scale
		width = type_modifiers[0]->Cast<ConstantExpression>().value.GetValue<uint8_t>();
		scale = type_modifiers.size() == 2 ? type_modifiers[1]->Cast<ConstantExpression>().value.GetValue<uint8_t>() : 0;
	}
	auto type_info = make_shared_ptr<DecimalTypeInfo>(width, scale);
	return LogicalType(LogicalTypeId::DECIMAL, type_info);
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformTypeModifiers(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<unique_ptr<ParsedExpression>> result;
	auto extract_list = ExtractResultFromParens(list_pr.Child<ListParseResult>(0))->Cast<OptionalParseResult>();
	if (extract_list.HasResult()) {
		auto expressions = ExtractParseResultsFromList(extract_list.optional_result);
		for (auto expression : expressions) {
			result.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expression));
		}
	}

	return result;
}

LogicalType PEGTransformerFactory::TransformSimpleType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto qualified_type_or_character = list_pr.Child<ListParseResult>(0);
	LogicalType result = transformer.Transform<LogicalType>(qualified_type_or_character.Child<ChoiceParseResult>(0).result);
	auto opt_modifiers = list_pr.Child<OptionalParseResult>(1);
	vector<unique_ptr<ParsedExpression>> modifiers;
	if (opt_modifiers.HasResult()) {
		modifiers = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(opt_modifiers.optional_result);
	}
	// TODO(Dtenwolde) add modifiers
	return result;
}


LogicalType PEGTransformerFactory::TransformQualifiedTypeName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// TODO(Dtenwolde) figure out what to do with qualified names
	auto &list_pr = parse_result->Cast<ListParseResult>();
	QualifiedName result;
	auto catalog_pr = list_pr.Child<OptionalParseResult>(0);
	if (catalog_pr.HasResult()) {
		result.catalog = transformer.Transform<string>(catalog_pr.optional_result);
	}
	auto schema_pr = list_pr.Child<OptionalParseResult>(1);
	if (schema_pr.HasResult()) {
		result.schema = transformer.Transform<string>(schema_pr.optional_result);
	}

	if (list_pr.children[2]->type == ParseResultType::IDENTIFIER) {
		result.name = list_pr.Child<IdentifierParseResult>(2).identifier;
	} else {
		result.name = transformer.Transform<string>(list_pr.Child<ListParseResult>(2));
	}
	return LogicalType(TransformStringToLogicalTypeId(result.name));

}

LogicalType PEGTransformerFactory::TransformCharacterType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	return LogicalType(LogicalTypeId::VARCHAR);
}

LogicalType PEGTransformerFactory::TransformMapType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1));
	auto type_list = ExtractParseResultsFromList(extract_parens);
	if (type_list.size() != 2) {
		throw ParserException("Map type needs exactly two entries, key and value type.");
	}
	auto key_type = transformer.Transform<LogicalType>(type_list[0]);
	auto value_type = transformer.Transform<LogicalType>(type_list[1]);
	return LogicalType::MAP(key_type, value_type);
}

LogicalType PEGTransformerFactory::TransformRowType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto colid_list = transformer.Transform<child_list_t<LogicalType>>(list_pr.Child<ListParseResult>(1));
	return LogicalType::STRUCT(colid_list);
}

LogicalType PEGTransformerFactory::TransformUnionType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto colid_list = transformer.Transform<child_list_t<LogicalType>>(list_pr.Child<ListParseResult>(1));
	return LogicalType::UNION(colid_list);
}

child_list_t<LogicalType> PEGTransformerFactory::TransformColIdTypeList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_list = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto colid_type_list = ExtractParseResultsFromList(extract_list);

	child_list_t<LogicalType> result;
	for (auto colid_type : colid_type_list) {
		result.push_back(transformer.Transform<std::pair<std::string, LogicalType>>(colid_type));
	}
	return result;
}

std::pair<std::string, LogicalType> PEGTransformerFactory::TransformColIdType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto colid = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto type = transformer.Transform<LogicalType>(list_pr.Child<ListParseResult>(1));
	return std::make_pair(colid, type);
}

LogicalType PEGTransformerFactory::TransformBitType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &opt_varying = list_pr.Child<OptionalParseResult>(1);
	if (opt_varying.HasResult()) {
		throw ParserException("Type with name varbit does not exist.");
	}
	auto &opt_modifiers = list_pr.Child<OptionalParseResult>(2);
	if (opt_modifiers.HasResult()) {
		throw ParserException("Type BIT does not support any modifiers.");
	}
	return LogicalType(LogicalTypeId::BIT);
}

LogicalType PEGTransformerFactory::TransformIntervalType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<LogicalType>(list_pr.Child<ChoiceParseResult>(0).result);
}

LogicalType PEGTransformerFactory::TransformIntervalInterval(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto opt_interval = list_pr.Child<OptionalParseResult>(1);
	if (opt_interval.HasResult()) {
		return transformer.Transform<LogicalType>(opt_interval.optional_result);
	} else {
		return LogicalType(LogicalTypeId::INTERVAL);
	}
}

LogicalType PEGTransformerFactory::TransformInterval(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// TODO(Dtenwolde) I think we need to do more here, but I cannot figure it out at the moment.
	// See transform_typename.cpp and interval_type.hpp
	return LogicalType(LogicalTypeId::INTERVAL);
}
} // namespace duckdb
