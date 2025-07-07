namespace duckdb {
QualifiedName PEGTransformerFactory::TransformDottedIdentifier(PEGTransformer &, ParseResult &parse_result) {
	// Rule: Identifier ('.' Identifier)*
	auto &list_pr = parse_result.Cast<ListParseResult>();
	vector<string> parts;

	// Add the first identifier
	parts.push_back(list_pr.Child<IdentifierParseResult>(0).identifier);

	// Add the rest of the identifiers from the optional repetition
	auto &repetition_list = list_pr.Child<ListParseResult>(1);
	for (auto &child_ref : repetition_list.children) {
		// Each child in the repetition is a ListParseResult from the sequence "'.' Identifier"
		auto &sub_list = child_ref.get().Cast<ListParseResult>();
		parts.push_back(sub_list.Child<IdentifierParseResult>(1).identifier);
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
