
namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformUseStatement(PEGTransformer &transformer,
                                                                      optional_ptr<ParseResult> parse_result) {
	// Rule: 'USE'i UseTarget
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &use_target_pr = list_pr.children[1];

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

QualifiedName PEGTransformerFactory::TransformUseTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	string qualified_name;
	if (choice_pr.result->type == ParseResultType::LIST) {
		auto use_target_children = choice_pr.result->Cast<ListParseResult>();
		for (auto& child : use_target_children.children) {
			if (child->type == ParseResultType::IDENTIFIER) {
				qualified_name += child->Cast<IdentifierParseResult>().identifier;
			}
			else if (child->type == ParseResultType::KEYWORD) {
				qualified_name += child->Cast<KeywordParseResult>().keyword;
			}
		}
	} else {
		qualified_name = choice_pr.result->Cast<IdentifierParseResult>().identifier;
	}
	auto result = QualifiedName::Parse(qualified_name);
	return result;
}


unique_ptr<SQLStatement> PEGTransformerFactory::TransformSetStatement(PEGTransformer &transformer,
                                                                      optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &child_pr = list_pr.Child<ListParseResult>(1);
	return transformer.Transform<unique_ptr<SQLStatement>>(child_pr.Child<ChoiceParseResult>(0));
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformResetStatement(PEGTransformer &transformer,
                                                                        optional_ptr<ParseResult> parse_result) {
	// Composer: 'RESET' (SetVariable / SetSetting)
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &child_pr = list_pr.children[1]->Cast<ChoiceParseResult>();

	SettingInfo setting_info = transformer.Transform<SettingInfo>(child_pr.result);
	return make_uniq<ResetVariableStatement>(setting_info.name, setting_info.scope);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformStandardAssignment(PEGTransformer &transformer,
                                                                            optional_ptr<ParseResult> parse_result) {
	// Composer: (SetVariable / SetSetting) SetAssignment
	auto &choice_pr = parse_result->Cast<ChoiceParseResult>();
	auto &list_pr = choice_pr.result->Cast<ListParseResult>();
	auto &setting_or_var_pr = list_pr.children[0]->Cast<ChoiceParseResult>();
	auto &set_assignment_pr = list_pr.children[1];

	SettingInfo setting_info = transformer.Transform<SettingInfo>(setting_or_var_pr.result);
	unique_ptr<ParsedExpression> value = transformer.Transform<unique_ptr<ParsedExpression>>(set_assignment_pr);

	return make_uniq<SetVariableStatement>(setting_info.name, std::move(value), setting_info.scope);
}

SettingInfo PEGTransformerFactory::TransformSettingOrVariable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// Dispatcher: SetVariable / SetSetting
	auto &choice_pr = parse_result->Cast<ChoiceParseResult>();
	auto &matched_child = choice_pr.result;
	return transformer.Transform<SettingInfo>(matched_child);
}

SettingInfo PEGTransformerFactory::TransformSetSetting(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// Leaf: SettingScope? SettingName
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &optional_scope_pr = list_pr.Child<OptionalParseResult>(0);

	SettingInfo result;
	result.name = transformer.Transform<string>(&list_pr.Child<ChoiceParseResult>(1));
	if (optional_scope_pr.optional_result) {
		result.scope = transformer.TransformEnum<SetScope>(optional_scope_pr.optional_result);
	}
	return result;
}

SettingInfo PEGTransformerFactory::TransformSetVariable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// Leaf: VariableScope SettingName
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &scope_pr = list_pr.children[0];

	SettingInfo result;
	result.name = transformer.Transform<string>(&list_pr.Child<ChoiceParseResult>(1));
	result.scope = transformer.TransformEnum<SetScope>(scope_pr);
	return result;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformSetAssignment(PEGTransformer &transformer,
                                                                           optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &variable_list_pr = list_pr.children[1];
	return transformer.Transform<vector<unique_ptr<ParsedExpression>>>(variable_list_pr);
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformVariableList(PEGTransformer &transformer,
                                                                          optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<unique_ptr<ParsedExpression>> expressions;
	expressions.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.children[0]));
	idx_t child_idx = 1;
	while (!list_pr.children[child_idx]->name.empty() && list_pr.children[child_idx]->type == ParseResultType::LIST) {
		expressions.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.children[child_idx]));
		child_idx++;
	}
	return expressions;
}

} // namespace duckdb
