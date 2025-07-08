namespace duckdb {

string PEGTransformerFactory::TransformNumberLiteral(PEGTransformer &transformer, ParseResult &parse_result) {
	throw NotImplementedException("TransformerFactory::TransformNumberLiteral");
}

string PEGTransformerFactory::TransformIdentifierOrKeyword(PEGTransformer &transformer, ParseResult &parse_result) {
	if (parse_result.type == ParseResultType::IDENTIFIER) {
		// Base case: It's a plain or quoted identifier.
		return parse_result.Cast<IdentifierParseResult>().identifier;
	}
	if (parse_result.type == ParseResultType::KEYWORD) {
		// Base case: It's a keyword that can be used as an identifier.
		return parse_result.Cast<KeywordParseResult>().keyword;
	}
	if (parse_result.type == ParseResultType::CHOICE) {
		// It's a choice between other rules (e.g., ColId <- UnreservedKeyword / Identifier).
		// We unwrap the choice and recursively transform the result.
		auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
		return transformer.Transform<string>(choice_pr.result.get());
	}
	if (parse_result.type == ParseResultType::LIST) {
		auto &list_pr = parse_result.Cast<ListParseResult>();
		// Find the meaningful child in the list. Predicates produce empty lists,
		// so we look for the non-empty result.
		for (auto &child : list_pr.children) {
			if (child.get().type == ParseResultType::LIST && child.get().Cast<ListParseResult>().children.empty()) {
				// This is an empty list from a successful predicate, ignore it.
				continue;
			}
			// This is the actual result (e.g., the IdentifierParseResult).
			// Recursively transform it to get the final string.
			return child.get().Cast<IdentifierParseResult>().identifier;
		}
	}
	throw ParserException("Unexpected ParseResult type in identifier transformation.");
}

QualifiedName PEGTransformerFactory::TransformDottedIdentifier(PEGTransformer &transformer, ParseResult &parse_result) {
	// Rule: ColId ('.' ColLabel)*
	auto &list_pr = parse_result.Cast<ListParseResult>();
	vector<string> parts;

	auto &col_id_pr = list_pr.children[0].get();
	parts.push_back(transformer.Transform<string>(col_id_pr));

	auto &repetition_list = list_pr.Child<ListParseResult>(1);
	for (auto &child_ref : repetition_list.children) {
		// Each child is a sequence: "'.' ColLabel"
		auto &sub_list = child_ref.get().Cast<ListParseResult>();
		auto &col_label_pr = sub_list.children[1].get();
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

}
