
namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformUseStatement(PEGTransformer &transformer,
                                                                      ParseResult &parse_result) {
	// Rule: 'USE'i UseTarget
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &use_target_pr = list_pr.children[1].get();

	QualifiedName qn = transformer.Transform<QualifiedName>(use_target_pr);

	if (!qn.catalog.empty()) {
		throw ParserException("Expected \"USE database\" or \"USE database.schema\"");
	}

	string value_str;
	if (qn.schema.empty()) {
		value_str = KeywordHelper::WriteOptionallyQuoted(qn.name, '"');
	} else {
		value_str = KeywordHelper::WriteOptionallyQuoted(qn.schema, '"') + "." +
		            KeywordHelper::WriteOptionallyQuoted(qn.name, '"');
	}

	auto value_expr = make_uniq<ConstantExpression>(Value(value_str));
	return make_uniq<SetVariableStatement>("schema", std::move(value_expr), SetScope::AUTOMATIC);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformSetStatement(PEGTransformer &transformer,
                                                                      ParseResult &parse_result) {
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(1);
	auto &matched_child = choice_pr.result.get();

	if (matched_child.name == "StandardAssignment") {
		return transformer.Transform<unique_ptr<SQLStatement>>(matched_child);
	}
	// Handle SetTimeZone
	throw NotImplementedException("SET TIME ZONE is not yet implemented.");
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformResetStatement(PEGTransformer &transformer,
                                                                        ParseResult &parse_result) {
	// Composer: 'RESET' (SetVariable / SetSetting)
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &child_pr = list_pr.children[1].get().Cast<ChoiceParseResult>();

	SettingInfo setting_info = transformer.Transform<SettingInfo>(child_pr.result);
	return make_uniq<ResetVariableStatement>(setting_info.name, setting_info.scope);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformStandardAssignment(PEGTransformer &transformer,
                                                                            ParseResult &parse_result) {
	// Composer: (SetVariable / SetSetting) SetAssignment
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &setting_or_var_pr = list_pr.children[0].get().Cast<ChoiceParseResult>();
	auto &set_assignment_pr = list_pr.children[1];

	SettingInfo setting_info = transformer.Transform<SettingInfo>(setting_or_var_pr.result);
	unique_ptr<ParsedExpression> value = transformer.Transform<unique_ptr<ParsedExpression>>(set_assignment_pr);

	return make_uniq<SetVariableStatement>(setting_info.name, std::move(value), setting_info.scope);
}

SettingInfo PEGTransformerFactory::TransformSettingOrVariable(PEGTransformer &transformer, ParseResult &parse_result) {
	// Dispatcher: SetVariable / SetSetting
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
	auto &matched_child = choice_pr.result.get();
	return transformer.Transform<SettingInfo>(matched_child);
}

SettingInfo PEGTransformerFactory::TransformSetSetting(PEGTransformer &transformer, ParseResult &parse_result) {
	// Leaf: SettingScope? SettingName
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &optional_scope_pr = list_pr.Child<OptionalParseResult>(0);

	SettingInfo result;
	result.name = transformer.Transform<string>(list_pr.Child<ChoiceParseResult>(1));
	if (optional_scope_pr.optional_result) {
		result.scope = transformer.TransformEnum<SetScope>(*optional_scope_pr.optional_result);
	}
	return result;
}

SettingInfo PEGTransformerFactory::TransformSetVariable(PEGTransformer &transformer, ParseResult &parse_result) {
	// Leaf: VariableScope SettingName
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &scope_pr = list_pr.children[0];

	SettingInfo result;
	result.name = transformer.Transform<string>(list_pr.Child<ChoiceParseResult>(1));
	result.scope = transformer.TransformEnum<SetScope>(scope_pr);
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSetAssignment(PEGTransformer &transformer,
                                                                           ParseResult &parse_result) {
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &variable_list_pr = list_pr.children[1];
	return transformer.Transform<unique_ptr<ParsedExpression>>(variable_list_pr);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformVariableList(PEGTransformer &transformer,
                                                                          ParseResult &parse_result) {
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &expression_pr = list_pr.children[0];
	return transformer.Transform<unique_ptr<ParsedExpression>>(expression_pr);
}

} // namespace duckdb
