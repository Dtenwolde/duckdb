namespace duckdb {

string PEGTransformerFactory::TransformIdentifierOrKeyword(PEGTransformer &transformer,
                                                           optional_ptr<ParseResult> parse_result) {
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
				if (choice_pr.result->type == ParseResultType::IDENTIFIER) {
					return choice_pr.result->Cast<IdentifierParseResult>().identifier;
				}
				if (choice_pr.result->type == ParseResultType::KEYWORD) {
					return choice_pr.result->Cast<KeywordParseResult>().keyword;
				}
				throw ParserException("Unexpected type encountered.");
			}
			if (child->type == ParseResultType::IDENTIFIER) {
				return child->Cast<IdentifierParseResult>().identifier;
			}
			throw InternalException("Unexpected IdentifierOrKeyword type encountered %s.",
			                        ParseResultToString(child->type));
		}
	}
	throw ParserException("Unexpected ParseResult type in identifier transformation.");
}

vector<string> PEGTransformerFactory::TransformDottedIdentifier(PEGTransformer &transformer,
                                                               optional_ptr<ParseResult> parse_result) {
	// Rule: ColId ('.' ColLabel)*
	// TODO(Dtenwolde): Should probably not return a qualifiedName (what about struct e.g.x.y.z)
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<string> parts;

	auto &col_id_pr = list_pr.children[0];
	if (col_id_pr->type == ParseResultType::IDENTIFIER) {
		parts.push_back(col_id_pr->Cast<IdentifierParseResult>().identifier);
	} else {
		parts.push_back(transformer.Transform<string>(col_id_pr));
	}

	auto &optional_elements = list_pr.Child<OptionalParseResult>(1);
	if (optional_elements.HasResult()) {
		auto repeat_elements = optional_elements.optional_result->Cast<RepeatParseResult>();
		for (auto &child_ref : repeat_elements.children) {
			// Each child is a sequence: "'.' ColLabel"
			auto &sub_list = child_ref->Cast<ListParseResult>();
			auto &col_label_pr = sub_list.children[1];
			if (col_label_pr->type == ParseResultType::IDENTIFIER) {
				parts.push_back(col_label_pr->Cast<IdentifierParseResult>().identifier);
			} else {
				parts.push_back(transformer.Transform<string>(col_label_pr));
			}
		}
	}
	return parts;
}

} // namespace duckdb
