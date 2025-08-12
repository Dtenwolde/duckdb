namespace duckdb {

string PEGTransformerFactory::TransformIdentifierOrKeyword(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	if (parse_result->type == ParseResultType::IDENTIFIER) {
		return parse_result->Cast<IdentifierParseResult>().identifier;
	}
	if (parse_result->type == ParseResultType::KEYWORD) {
		return parse_result->Cast<KeywordParseResult>().keyword;
	}
	if (parse_result->type == ParseResultType::CHOICE) {
		auto &choice_pr = parse_result->Cast<ChoiceParseResult>();
		return transformer.Transform<string>(choice_pr.result);
	}
	if (parse_result->type == ParseResultType::LIST) {
		auto &list_pr = parse_result->Cast<ListParseResult>();
		for (auto &child : list_pr.children) {
			if (child->type == ParseResultType::LIST && child->Cast<ListParseResult>().children.empty()) {
				continue;
			}
			if (child->type == ParseResultType::CHOICE) {
				auto &choice_pr = child->Cast<ChoiceParseResult>();
				return choice_pr.result->Cast<KeywordParseResult>().keyword;
			}
			if (child->type == ParseResultType::IDENTIFIER) {
				return child->Cast<IdentifierParseResult>().identifier;
			}
			throw InternalException("Unexpected IdentifierOrKeyword type encountered %s.", ParseResultToString(child->type));
		}
	}
	throw ParserException("Unexpected ParseResult type in identifier transformation.");
}

QualifiedName PEGTransformerFactory::TransformDottedIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// Rule: ColId ('.' ColLabel)*
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<string> parts;

	auto &col_id_pr = list_pr.children[0];
	parts.push_back(transformer.Transform<string>(col_id_pr));

	auto &repetition_list = list_pr.Child<ListParseResult>(1);
	for (auto &child_ref : repetition_list.children) {
		// Each child is a sequence: "'.' ColLabel"
		auto &sub_list = child_ref->Cast<ListParseResult>();
		auto &col_label_pr = sub_list.children[1];
		parts.push_back(transformer.Transform<string>(col_label_pr));
	}

	QualifiedName result;
	if (parts.size() == 1) {
		result.name = parts[0];
	} else if (parts.size() == 2) {
		result.schema = parts[0];
		result.name = parts[1];
	} else if (parts.size() == 3) {
		result.catalog = parts[0];
		result.schema = parts[1];
		result.name = parts[2];
	} else if (parts.size() > 3) {
		throw ParserException("Too many parts in identifier, expected a maximum of 3 (catalog.schema.name)");
	}
	return result;
}

} // namespace duckdb
