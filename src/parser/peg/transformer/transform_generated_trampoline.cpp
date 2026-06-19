#include "duckdb/parser/peg/transformer/peg_transformer.hpp"

namespace duckdb {

// BEGIN generated trampoline transformer rules
const TransformFrameOps PEGTransformerFactory::ANALYZE_STATEMENT_OPS = {
    "AnalyzeStatement", &PEGTransformerFactory::InitializeAnalyzeStatement,
    &PEGTransformerFactory::FinalizeAnalyzeStatement};
const TransformFrameOps PEGTransformerFactory::ANALYZE_TARGET_OPS = {
    "AnalyzeTarget", &PEGTransformerFactory::InitializeAnalyzeTarget,
    &PEGTransformerFactory::FinalizeAnalyzeTarget};
const TransformFrameOps PEGTransformerFactory::ANALYZE_VERBOSE_OPS = {
    "AnalyzeVerbose", &PEGTransformerFactory::InitializeAnalyzeVerbose,
    &PEGTransformerFactory::FinalizeAnalyzeVerbose};
const TransformFrameOps PEGTransformerFactory::CHECKPOINT_STATEMENT_OPS = {
    "CheckpointStatement", &PEGTransformerFactory::InitializeCheckpointStatement,
    &PEGTransformerFactory::FinalizeCheckpointStatement};
const TransformFrameOps PEGTransformerFactory::CHECKPOINT_FORCE_OPS = {
    "CheckpointForce", &PEGTransformerFactory::InitializeCheckpointForce,
    &PEGTransformerFactory::FinalizeCheckpointForce};
const TransformFrameOps PEGTransformerFactory::COMMENT_STATEMENT_OPS = {
    "CommentStatement", &PEGTransformerFactory::InitializeCommentStatement,
    &PEGTransformerFactory::FinalizeCommentStatement};
const TransformFrameOps PEGTransformerFactory::COMMENT_ON_TYPE_OPS = {
    "CommentOnType", &PEGTransformerFactory::InitializeCommentOnType,
    &PEGTransformerFactory::FinalizeCommentOnType};
const TransformFrameOps PEGTransformerFactory::COMMENT_TABLE_OPS = {
    "CommentTable", &PEGTransformerFactory::InitializeCommentTable,
    &PEGTransformerFactory::FinalizeCommentTable};
const TransformFrameOps PEGTransformerFactory::COMMENT_SEQUENCE_OPS = {
    "CommentSequence", &PEGTransformerFactory::InitializeCommentSequence,
    &PEGTransformerFactory::FinalizeCommentSequence};
const TransformFrameOps PEGTransformerFactory::COMMENT_FUNCTION_OPS = {
    "CommentFunction", &PEGTransformerFactory::InitializeCommentFunction,
    &PEGTransformerFactory::FinalizeCommentFunction};
const TransformFrameOps PEGTransformerFactory::COMMENT_MACRO_TABLE_OPS = {
    "CommentMacroTable", &PEGTransformerFactory::InitializeCommentMacroTable,
    &PEGTransformerFactory::FinalizeCommentMacroTable};
const TransformFrameOps PEGTransformerFactory::COMMENT_MACRO_OPS = {
    "CommentMacro", &PEGTransformerFactory::InitializeCommentMacro,
    &PEGTransformerFactory::FinalizeCommentMacro};
const TransformFrameOps PEGTransformerFactory::COMMENT_VIEW_OPS = {
    "CommentView", &PEGTransformerFactory::InitializeCommentView,
    &PEGTransformerFactory::FinalizeCommentView};
const TransformFrameOps PEGTransformerFactory::COMMENT_DATABASE_OPS = {
    "CommentDatabase", &PEGTransformerFactory::InitializeCommentDatabase,
    &PEGTransformerFactory::FinalizeCommentDatabase};
const TransformFrameOps PEGTransformerFactory::COMMENT_INDEX_OPS = {
    "CommentIndex", &PEGTransformerFactory::InitializeCommentIndex,
    &PEGTransformerFactory::FinalizeCommentIndex};
const TransformFrameOps PEGTransformerFactory::COMMENT_SCHEMA_OPS = {
    "CommentSchema", &PEGTransformerFactory::InitializeCommentSchema,
    &PEGTransformerFactory::FinalizeCommentSchema};
const TransformFrameOps PEGTransformerFactory::COMMENT_TYPE_OPS = {
    "CommentType", &PEGTransformerFactory::InitializeCommentType,
    &PEGTransformerFactory::FinalizeCommentType};
const TransformFrameOps PEGTransformerFactory::COMMENT_COLUMN_OPS = {
    "CommentColumn", &PEGTransformerFactory::InitializeCommentColumn,
    &PEGTransformerFactory::FinalizeCommentColumn};
const TransformFrameOps PEGTransformerFactory::COMMENT_VALUE_OPS = {
    "CommentValue", &PEGTransformerFactory::InitializeCommentValue,
    &PEGTransformerFactory::FinalizeCommentValue};
const TransformFrameOps PEGTransformerFactory::CONNECT_STATEMENT_OPS = {
    "ConnectStatement", &PEGTransformerFactory::InitializeConnectStatement,
    &PEGTransformerFactory::FinalizeConnectStatement};
const TransformFrameOps PEGTransformerFactory::DISCONNECT_STATEMENT_OPS = {
    "DisconnectStatement", &PEGTransformerFactory::InitializeDisconnectStatement,
    &PEGTransformerFactory::FinalizeDisconnectStatement};
const TransformFrameOps PEGTransformerFactory::SESSION_TARGET_OPS = {
    "SessionTarget", &PEGTransformerFactory::InitializeSessionTarget,
    &PEGTransformerFactory::FinalizeSessionTarget};
const TransformFrameOps PEGTransformerFactory::LOCAL_SESSION_TARGET_OPS = {
    "LocalSessionTarget", &PEGTransformerFactory::InitializeLocalSessionTarget,
    &PEGTransformerFactory::FinalizeLocalSessionTarget};
const TransformFrameOps PEGTransformerFactory::STRING_SESSION_TARGET_OPS = {
    "StringSessionTarget", &PEGTransformerFactory::InitializeStringSessionTarget,
    &PEGTransformerFactory::FinalizeStringSessionTarget};
const TransformFrameOps PEGTransformerFactory::CATALOG_SESSION_TARGET_OPS = {
    "CatalogSessionTarget", &PEGTransformerFactory::InitializeCatalogSessionTarget,
    &PEGTransformerFactory::FinalizeCatalogSessionTarget};
const TransformFrameOps PEGTransformerFactory::DEALLOCATE_STATEMENT_OPS = {
    "DeallocateStatement", &PEGTransformerFactory::InitializeDeallocateStatement,
    &PEGTransformerFactory::FinalizeDeallocateStatement};
const TransformFrameOps PEGTransformerFactory::DEALLOCATE_PREPARE_OPS = {
    "DeallocatePrepare", &PEGTransformerFactory::InitializeDeallocatePrepare,
    &PEGTransformerFactory::FinalizeDeallocatePrepare};
const TransformFrameOps PEGTransformerFactory::DETACH_STATEMENT_OPS = {
    "DetachStatement", &PEGTransformerFactory::InitializeDetachStatement,
    &PEGTransformerFactory::FinalizeDetachStatement};
const TransformFrameOps PEGTransformerFactory::LOAD_STATEMENT_OPS = {
    "LoadStatement", &PEGTransformerFactory::InitializeLoadStatement,
    &PEGTransformerFactory::FinalizeLoadStatement};
const TransformFrameOps PEGTransformerFactory::EXTENSION_ALIAS_OPS = {
    "ExtensionAlias", &PEGTransformerFactory::InitializeExtensionAlias,
    &PEGTransformerFactory::FinalizeExtensionAlias};
const TransformFrameOps PEGTransformerFactory::INSTALL_STATEMENT_OPS = {
    "InstallStatement", &PEGTransformerFactory::InitializeInstallStatement,
    &PEGTransformerFactory::FinalizeInstallStatement};
const TransformFrameOps PEGTransformerFactory::UPDATE_EXTENSIONS_STATEMENT_OPS = {
    "UpdateExtensionsStatement", &PEGTransformerFactory::InitializeUpdateExtensionsStatement,
    &PEGTransformerFactory::FinalizeUpdateExtensionsStatement};
const TransformFrameOps PEGTransformerFactory::FROM_SOURCE_OPS = {
    "FromSource", &PEGTransformerFactory::InitializeFromSource,
    &PEGTransformerFactory::FinalizeFromSource};
const TransformFrameOps PEGTransformerFactory::FROM_SOURCE_IDENTIFIER_OPS = {
    "FromSourceIdentifier", &PEGTransformerFactory::InitializeFromSourceIdentifier,
    &PEGTransformerFactory::FinalizeFromSourceIdentifier};
const TransformFrameOps PEGTransformerFactory::FROM_SOURCE_STRING_OPS = {
    "FromSourceString", &PEGTransformerFactory::InitializeFromSourceString,
    &PEGTransformerFactory::FinalizeFromSourceString};
const TransformFrameOps PEGTransformerFactory::VERSION_NUMBER_OPS = {
    "VersionNumber", &PEGTransformerFactory::InitializeVersionNumber,
    &PEGTransformerFactory::FinalizeVersionNumber};
const TransformFrameOps PEGTransformerFactory::TRANSACTION_STATEMENT_OPS = {
    "TransactionStatement", &PEGTransformerFactory::InitializeTransactionStatement,
    &PEGTransformerFactory::FinalizeTransactionStatement};
const TransformFrameOps PEGTransformerFactory::BEGIN_TRANSACTION_OPS = {
    "BeginTransaction", &PEGTransformerFactory::InitializeBeginTransaction,
    &PEGTransformerFactory::FinalizeBeginTransaction};
const TransformFrameOps PEGTransformerFactory::ROLLBACK_TRANSACTION_OPS = {
    "RollbackTransaction", &PEGTransformerFactory::InitializeRollbackTransaction,
    &PEGTransformerFactory::FinalizeRollbackTransaction};
const TransformFrameOps PEGTransformerFactory::COMMIT_TRANSACTION_OPS = {
    "CommitTransaction", &PEGTransformerFactory::InitializeCommitTransaction,
    &PEGTransformerFactory::FinalizeCommitTransaction};
const TransformFrameOps PEGTransformerFactory::READ_OR_WRITE_OPS = {
    "ReadOrWrite", &PEGTransformerFactory::InitializeReadOrWrite,
    &PEGTransformerFactory::FinalizeReadOrWrite};
const TransformFrameOps PEGTransformerFactory::READ_ONLY_OR_READ_WRITE_OPS = {
    "ReadOnlyOrReadWrite", &PEGTransformerFactory::InitializeReadOnlyOrReadWrite,
    &PEGTransformerFactory::FinalizeReadOnlyOrReadWrite};
const TransformFrameOps PEGTransformerFactory::READ_ONLY_OPS = {
    "ReadOnly", &PEGTransformerFactory::InitializeReadOnly,
    &PEGTransformerFactory::FinalizeReadOnly};
const TransformFrameOps PEGTransformerFactory::READ_WRITE_OPS = {
    "ReadWrite", &PEGTransformerFactory::InitializeReadWrite,
    &PEGTransformerFactory::FinalizeReadWrite};
const TransformFrameOps PEGTransformerFactory::USE_STATEMENT_OPS = {
    "UseStatement", &PEGTransformerFactory::InitializeUseStatement,
    &PEGTransformerFactory::FinalizeUseStatement};
const TransformFrameOps PEGTransformerFactory::USE_TARGET_OPS = {
    "UseTarget", &PEGTransformerFactory::InitializeUseTarget,
    &PEGTransformerFactory::FinalizeUseTarget};
const TransformFrameOps PEGTransformerFactory::SCHEMA_NAME_AS_USE_TARGET_OPS = {
    "SchemaNameAsUseTarget", &PEGTransformerFactory::InitializeSchemaNameAsUseTarget,
    &PEGTransformerFactory::FinalizeSchemaNameAsUseTarget};
const TransformFrameOps PEGTransformerFactory::CATALOG_NAME_AS_USE_TARGET_OPS = {
    "CatalogNameAsUseTarget", &PEGTransformerFactory::InitializeCatalogNameAsUseTarget,
    &PEGTransformerFactory::FinalizeCatalogNameAsUseTarget};
const TransformFrameOps PEGTransformerFactory::USE_TARGET_CATALOG_SCHEMA_OPS = {
    "UseTargetCatalogSchema", &PEGTransformerFactory::InitializeUseTargetCatalogSchema,
    &PEGTransformerFactory::FinalizeUseTargetCatalogSchema};
const TransformFrameOps PEGTransformerFactory::DOT_IDENTIFIER_OPS = {
    "DotIdentifier", &PEGTransformerFactory::InitializeDotIdentifier,
    &PEGTransformerFactory::FinalizeDotIdentifier};
const TransformFrameOps PEGTransformerFactory::VACUUM_STATEMENT_OPS = {
    "VacuumStatement", &PEGTransformerFactory::InitializeVacuumStatement,
    &PEGTransformerFactory::FinalizeVacuumStatement};
const TransformFrameOps PEGTransformerFactory::VACUUM_OPTIONS_OPS = {
    "VacuumOptions", &PEGTransformerFactory::InitializeVacuumOptions,
    &PEGTransformerFactory::FinalizeVacuumOptions};
const TransformFrameOps PEGTransformerFactory::VACUUM_PARENS_OPTIONS_OPS = {
    "VacuumParensOptions", &PEGTransformerFactory::InitializeVacuumParensOptions,
    &PEGTransformerFactory::FinalizeVacuumParensOptions};
const TransformFrameOps PEGTransformerFactory::VACUUM_LEGACY_OPTIONS_OPS = {
    "VacuumLegacyOptions", &PEGTransformerFactory::InitializeVacuumLegacyOptions,
    &PEGTransformerFactory::FinalizeVacuumLegacyOptions};
const TransformFrameOps PEGTransformerFactory::VACUUM_OPTION_OPS = {
    "VacuumOption", &PEGTransformerFactory::InitializeVacuumOption,
    &PEGTransformerFactory::FinalizeVacuumOption};
const TransformFrameOps PEGTransformerFactory::OPT_ANALYZE_OPS = {
    "OptAnalyze", &PEGTransformerFactory::InitializeOptAnalyze,
    &PEGTransformerFactory::FinalizeOptAnalyze};
const TransformFrameOps PEGTransformerFactory::OPT_FULL_OPS = {
    "OptFull", &PEGTransformerFactory::InitializeOptFull,
    &PEGTransformerFactory::FinalizeOptFull};
const TransformFrameOps PEGTransformerFactory::OPT_FREEZE_OPS = {
    "OptFreeze", &PEGTransformerFactory::InitializeOptFreeze,
    &PEGTransformerFactory::FinalizeOptFreeze};
const TransformFrameOps PEGTransformerFactory::OPT_VERBOSE_OPS = {
    "OptVerbose", &PEGTransformerFactory::InitializeOptVerbose,
    &PEGTransformerFactory::FinalizeOptVerbose};
const TransformFrameOps PEGTransformerFactory::NAME_LIST_OPS = {
    "NameList", &PEGTransformerFactory::InitializeNameList,
    &PEGTransformerFactory::FinalizeNameList};
const TransformFrameOps PEGTransformerFactory::BASE_TABLE_NAME_OPS = {
    "BaseTableName", &PEGTransformerFactory::InitializeBaseTableName,
    &PEGTransformerFactory::FinalizeBaseTableName};
const TransformFrameOps PEGTransformerFactory::DOTTED_IDENTIFIER_OPS = {
    "DottedIdentifier", &PEGTransformerFactory::InitializeDottedIdentifier,
    &PEGTransformerFactory::FinalizeDottedIdentifier};
const TransformFrameOps PEGTransformerFactory::NULL_LITERAL_OPS = {
    "NullLiteral", &PEGTransformerFactory::InitializeNullLiteral,
    &PEGTransformerFactory::FinalizeNullLiteral};
const TransformFrameOps PEGTransformerFactory::IF_EXISTS_OPS = {
    "IfExists", &PEGTransformerFactory::InitializeIfExists,
    &PEGTransformerFactory::FinalizeIfExists};
const TransformFrameOps PEGTransformerFactory::COL_ID_OR_STRING_OPS = {
    "ColIdOrString", &PEGTransformerFactory::InitializeColIdOrString,
    &PEGTransformerFactory::FinalizeColIdOrString};
const TransformFrameOps PEGTransformerFactory::IDENTIFIER_OR_STRING_LITERAL_OPS = {
    "IdentifierOrStringLiteral", &PEGTransformerFactory::InitializeIdentifierOrStringLiteral,
    &PEGTransformerFactory::FinalizeIdentifierOrStringLiteral};
const TransformFrameOps PEGTransformerFactory::COL_ID_OPS = {
    "ColId", &PEGTransformerFactory::InitializeColId,
    &PEGTransformerFactory::FinalizeColId};
const TransformFrameOps PEGTransformerFactory::CATALOG_RESERVED_SCHEMA_TABLE_OPS = {
    "CatalogReservedSchemaTable", &PEGTransformerFactory::InitializeCatalogReservedSchemaTable,
    &PEGTransformerFactory::FinalizeCatalogReservedSchemaTable};
const TransformFrameOps PEGTransformerFactory::SCHEMA_RESERVED_TABLE_OPS = {
    "SchemaReservedTable", &PEGTransformerFactory::InitializeSchemaReservedTable,
    &PEGTransformerFactory::FinalizeSchemaReservedTable};
const TransformFrameOps PEGTransformerFactory::UNQUALIFIED_BASE_TABLE_NAME_OPS = {
    "UnqualifiedBaseTableName", &PEGTransformerFactory::InitializeUnqualifiedBaseTableName,
    &PEGTransformerFactory::FinalizeUnqualifiedBaseTableName};
const TransformFrameOps PEGTransformerFactory::DOT_COL_LABEL_OPS = {
    "DotColLabel", &PEGTransformerFactory::InitializeDotColLabel,
    &PEGTransformerFactory::FinalizeDotColLabel};
const TransformFrameOps PEGTransformerFactory::CATALOG_QUALIFICATION_OPS = {
    "CatalogQualification", &PEGTransformerFactory::InitializeCatalogQualification,
    &PEGTransformerFactory::FinalizeCatalogQualification};
const TransformFrameOps PEGTransformerFactory::RESERVED_SCHEMA_QUALIFICATION_OPS = {
    "ReservedSchemaQualification", &PEGTransformerFactory::InitializeReservedSchemaQualification,
    &PEGTransformerFactory::FinalizeReservedSchemaQualification};
const TransformFrameOps PEGTransformerFactory::SCHEMA_QUALIFICATION_OPS = {
    "SchemaQualification", &PEGTransformerFactory::InitializeSchemaQualification,
    &PEGTransformerFactory::FinalizeSchemaQualification};

void PEGTransformerFactory::InitializeAnalyzeStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &analyze_verbose_opt_0 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (analyze_verbose_opt_0.HasResult()) {
		child_result_count++;
	}
	auto &analyze_target_opt_1 = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (analyze_target_opt_1.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (analyze_verbose_opt_0.HasResult()) {
		stack.PushFrame(analyze_verbose_opt_0.GetResult(), ANALYZE_VERBOSE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
	if (analyze_target_opt_1.HasResult()) {
		stack.PushFrame(analyze_target_opt_1.GetResult(), ANALYZE_TARGET_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeAnalyzeStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	optional<bool> analyze_verbose {};
	auto &analyze_verbose_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (analyze_verbose_opt.HasResult()) {
		auto analyze_verbose_value = frame.TakeResult<bool>(child_slot);
		analyze_verbose = analyze_verbose_value;
		child_slot++;
	}
	optional<AnalyzeTarget> analyze_target {};
	auto &analyze_target_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (analyze_target_opt.HasResult()) {
		auto analyze_target_value = frame.TakeResult<AnalyzeTarget>(child_slot);
		analyze_target = std::move(analyze_target_value);
		child_slot++;
	}
	auto result = TransformAnalyzeStatement(transformer, analyze_verbose, std::move(analyze_target));
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeAnalyzeTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	auto &name_list_opt_1 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (name_list_opt_1.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(0), BASE_TABLE_NAME_OPS, parent_frame_index, child_slot);
	child_slot++;
	if (name_list_opt_1.HasResult()) {
		stack.PushFrame(name_list_opt_1.GetResult(), NAME_LIST_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeAnalyzeTarget
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	auto base_table_name = frame.TakeResult<unique_ptr<BaseTableRef>>(child_slot);
	child_slot++;
	optional<vector<string>> name_list {};
	auto &name_list_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (name_list_opt.HasResult()) {
		auto name_list_value = frame.TakeResult<vector<string>>(child_slot);
		name_list = name_list_value;
		child_slot++;
	}
	auto result = TransformAnalyzeTarget(transformer, std::move(base_table_name), name_list);
	return make_uniq<TypedTransformResult<AnalyzeTarget>>(std::move(result));
}

void PEGTransformerFactory::InitializeAnalyzeVerbose(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeAnalyzeVerbose
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformAnalyzeVerbose(transformer);
	return make_uniq<TypedTransformResult<bool>>(result);
}

void PEGTransformerFactory::InitializeCheckpointStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &checkpoint_force_opt_0 = list_pr.GetChild(0).Cast<OptionalParseResult>();
	if (checkpoint_force_opt_0.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (checkpoint_force_opt_0.HasResult()) {
		stack.PushFrame(checkpoint_force_opt_0.GetResult(), CHECKPOINT_FORCE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCheckpointStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	optional<bool> checkpoint_force {};
	auto &checkpoint_force_opt = list_pr.GetChild(0).Cast<OptionalParseResult>();
	if (checkpoint_force_opt.HasResult()) {
		auto checkpoint_force_value = frame.TakeResult<bool>(child_slot);
		checkpoint_force = checkpoint_force_value;
		child_slot++;
	}
	optional<Identifier> catalog_name {};
	auto &catalog_name_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (catalog_name_opt.HasResult()) {
		auto catalog_name_value = catalog_name_opt.GetResult().Cast<IdentifierParseResult>().identifier;
		catalog_name = catalog_name_value;
	}
	auto result = TransformCheckpointStatement(transformer, checkpoint_force, catalog_name);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeCheckpointForce(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCheckpointForce
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCheckpointForce(transformer);
	return make_uniq<TypedTransformResult<bool>>(result);
}

void PEGTransformerFactory::InitializeCommentStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	child_result_count++;
	child_result_count++;
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(2), COMMENT_ON_TYPE_OPS, parent_frame_index, child_slot);
	child_slot++;
	stack.PushFrame(list_pr.GetChild(3), DOTTED_IDENTIFIER_OPS, parent_frame_index, child_slot);
	child_slot++;
	stack.PushFrame(list_pr.GetChild(5), COMMENT_VALUE_OPS, parent_frame_index, child_slot);
	child_slot++;
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	idx_t child_slot = 0;
	auto comment_on_type = frame.TakeResult<CatalogType>(child_slot);
	child_slot++;
	auto dotted_identifier = frame.TakeResult<vector<string>>(child_slot);
	child_slot++;
	auto comment_value = frame.TakeResult<Value>(child_slot);
	child_slot++;
	auto result = TransformCommentStatement(transformer, comment_on_type, dotted_identifier, comment_value);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeCommentOnType(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentOnType(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<CatalogType>(0);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentTable(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentTable
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentTable(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentSequence(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentSequence
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentSequence(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentFunction(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentFunction
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentFunction(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentMacroTable(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentMacroTable
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentMacroTable(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentMacro(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentMacro
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentMacro(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentView(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentView
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentView(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentDatabase(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentDatabase
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentDatabase(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentIndex(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentIndex
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentIndex(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentSchema(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentSchema
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentSchema(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentType(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentType
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentType(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentColumn(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentColumn
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformCommentColumn(transformer);
	return make_uniq<TypedTransformResult<CatalogType>>(result);
}

void PEGTransformerFactory::InitializeCommentValue(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommentValue(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	Value result;
	if (choice_pr.GetResult().type == ParseResultType::STRING) {
		result = Value(choice_pr.GetResult().Cast<StringLiteralParseResult>().result);
	} else {
		result = TransformNullLiteral(transformer);
	}
	return make_uniq<TypedTransformResult<Value>>(result);
}

void PEGTransformerFactory::InitializeConnectStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &session_target_opt_0 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (session_target_opt_0.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (session_target_opt_0.HasResult()) {
		stack.PushFrame(session_target_opt_0.GetResult(), SESSION_TARGET_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeConnectStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	optional<unique_ptr<ConnectInfo>> session_target {};
	auto &session_target_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (session_target_opt.HasResult()) {
		auto session_target_value = frame.TakeResult<unique_ptr<ConnectInfo>>(child_slot);
		session_target = std::move(session_target_value);
		child_slot++;
	}
	auto result = TransformConnectStatement(transformer, std::move(session_target));
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeDisconnectStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDisconnectStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformDisconnectStatement(transformer);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeSessionTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeSessionTarget(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<unique_ptr<ConnectInfo>>(0);
	return make_uniq<TypedTransformResult<unique_ptr<ConnectInfo>>>(std::move(result));
}

void PEGTransformerFactory::InitializeLocalSessionTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeLocalSessionTarget
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformLocalSessionTarget(transformer);
	return make_uniq<TypedTransformResult<unique_ptr<ConnectInfo>>>(std::move(result));
}

void PEGTransformerFactory::InitializeStringSessionTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeStringSessionTarget
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto string_literal = list_pr.GetChild(0).Cast<StringLiteralParseResult>().result;
	auto result = TransformStringSessionTarget(transformer, string_literal);
	return make_uniq<TypedTransformResult<unique_ptr<ConnectInfo>>>(std::move(result));
}

void PEGTransformerFactory::InitializeCatalogSessionTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCatalogSessionTarget
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto catalog_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformCatalogSessionTarget(transformer, catalog_name);
	return make_uniq<TypedTransformResult<unique_ptr<ConnectInfo>>>(std::move(result));
}

void PEGTransformerFactory::InitializeDeallocateStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &deallocate_prepare_opt_0 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (deallocate_prepare_opt_0.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (deallocate_prepare_opt_0.HasResult()) {
		stack.PushFrame(deallocate_prepare_opt_0.GetResult(), DEALLOCATE_PREPARE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDeallocateStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	optional<bool> deallocate_prepare {};
	auto &deallocate_prepare_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (deallocate_prepare_opt.HasResult()) {
		auto deallocate_prepare_value = frame.TakeResult<bool>(child_slot);
		deallocate_prepare = deallocate_prepare_value;
		child_slot++;
	}
	auto identifier = list_pr.GetChild(2).Cast<IdentifierParseResult>().identifier;
	auto result = TransformDeallocateStatement(transformer, deallocate_prepare, identifier);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeDeallocatePrepare(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDeallocatePrepare
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformDeallocatePrepare(transformer);
	return make_uniq<TypedTransformResult<bool>>(result);
}

void PEGTransformerFactory::InitializeDetachStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &if_exists_opt_0 = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (if_exists_opt_0.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (if_exists_opt_0.HasResult()) {
		stack.PushFrame(if_exists_opt_0.GetResult(), IF_EXISTS_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDetachStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	bool has_result {};
	auto &has_result_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	has_result = has_result_opt.HasResult();
	optional<bool> if_exists {};
	auto &if_exists_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (if_exists_opt.HasResult()) {
		auto if_exists_value = frame.TakeResult<bool>(child_slot);
		if_exists = if_exists_value;
		child_slot++;
	}
	auto catalog_name = list_pr.GetChild(3).Cast<IdentifierParseResult>().identifier;
	auto result = TransformDetachStatement(transformer, has_result, if_exists, catalog_name);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeLoadStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	auto &extension_alias_opt_1 = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (extension_alias_opt_1.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(1), COL_ID_OR_STRING_OPS, parent_frame_index, child_slot);
	child_slot++;
	if (extension_alias_opt_1.HasResult()) {
		stack.PushFrame(extension_alias_opt_1.GetResult(), EXTENSION_ALIAS_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeLoadStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	auto col_id_or_string = frame.TakeResult<Identifier>(child_slot);
	child_slot++;
	optional<Identifier> extension_alias {};
	auto &extension_alias_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (extension_alias_opt.HasResult()) {
		auto extension_alias_value = frame.TakeResult<Identifier>(child_slot);
		extension_alias = extension_alias_value;
		child_slot++;
	}
	auto result = TransformLoadStatement(transformer, col_id_or_string, extension_alias);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeExtensionAlias(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeExtensionAlias
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto identifier = list_pr.GetChild(1).Cast<IdentifierParseResult>().identifier;
	auto result = TransformExtensionAlias(transformer, identifier);
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

void PEGTransformerFactory::InitializeInstallStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	auto &from_source_opt_1 = list_pr.GetChild(3).Cast<OptionalParseResult>();
	if (from_source_opt_1.HasResult()) {
		child_result_count++;
	}
	auto &version_number_opt_2 = list_pr.GetChild(4).Cast<OptionalParseResult>();
	if (version_number_opt_2.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(2), IDENTIFIER_OR_STRING_LITERAL_OPS, parent_frame_index, child_slot);
	child_slot++;
	if (from_source_opt_1.HasResult()) {
		stack.PushFrame(from_source_opt_1.GetResult(), FROM_SOURCE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
	if (version_number_opt_2.HasResult()) {
		stack.PushFrame(version_number_opt_2.GetResult(), VERSION_NUMBER_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeInstallStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	bool has_result_0 {};
	auto &has_result_0_opt = list_pr.GetChild(0).Cast<OptionalParseResult>();
	has_result_0 = has_result_0_opt.HasResult();
	auto identifier_or_string_literal = frame.TakeResult<QualifiedName>(child_slot);
	child_slot++;
	optional<ExtensionRepositoryInfo> from_source {};
	auto &from_source_opt = list_pr.GetChild(3).Cast<OptionalParseResult>();
	if (from_source_opt.HasResult()) {
		auto from_source_value = frame.TakeResult<ExtensionRepositoryInfo>(child_slot);
		from_source = from_source_value;
		child_slot++;
	}
	optional<string> version_number {};
	auto &version_number_opt = list_pr.GetChild(4).Cast<OptionalParseResult>();
	if (version_number_opt.HasResult()) {
		auto version_number_value = frame.TakeResult<string>(child_slot);
		version_number = version_number_value;
		child_slot++;
	}
	auto result = TransformInstallStatement(transformer, has_result_0, identifier_or_string_literal, from_source, version_number);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeUpdateExtensionsStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUpdateExtensionsStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	optional<vector<Identifier>> identifier {};
	auto &identifier_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (identifier_opt.HasResult()) {
		vector<Identifier> identifier_value;
		auto identifier_items = ExtractParseResultsFromList(ExtractResultFromParens(identifier_opt.GetResult()));
		for (idx_t child_idx = 0; child_idx < identifier_items.size(); child_idx++) {
			auto &identifier_item = identifier_items[child_idx];
			auto identifier_child_value = identifier_item.get().Cast<IdentifierParseResult>().identifier;
			identifier_value.push_back(identifier_child_value);
		}
		identifier = std::move(identifier_value);
	}
	auto result = TransformUpdateExtensionsStatement(transformer, identifier);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeFromSource(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeFromSource(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<ExtensionRepositoryInfo>(0);
	return make_uniq<TypedTransformResult<ExtensionRepositoryInfo>>(result);
}

void PEGTransformerFactory::InitializeFromSourceIdentifier(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeFromSourceIdentifier
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto identifier = list_pr.GetChild(1).Cast<IdentifierParseResult>().identifier;
	auto result = TransformFromSourceIdentifier(transformer, identifier);
	return make_uniq<TypedTransformResult<ExtensionRepositoryInfo>>(result);
}

void PEGTransformerFactory::InitializeFromSourceString(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeFromSourceString
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto string_literal = list_pr.GetChild(1).Cast<StringLiteralParseResult>().result;
	auto result = TransformFromSourceString(transformer, string_literal);
	return make_uniq<TypedTransformResult<ExtensionRepositoryInfo>>(result);
}

void PEGTransformerFactory::InitializeVersionNumber(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(1), IDENTIFIER_OR_STRING_LITERAL_OPS, parent_frame_index, child_slot);
	child_slot++;
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeVersionNumber
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	idx_t child_slot = 0;
	auto identifier_or_string_literal = frame.TakeResult<QualifiedName>(child_slot);
	child_slot++;
	auto result = TransformVersionNumber(transformer, identifier_or_string_literal);
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeTransactionStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeTransactionStatement(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<unique_ptr<SQLStatement>>(0);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeBeginTransaction(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &read_or_write_opt_0 = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (read_or_write_opt_0.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (read_or_write_opt_0.HasResult()) {
		stack.PushFrame(read_or_write_opt_0.GetResult(), READ_OR_WRITE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeBeginTransaction
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	bool has_result {};
	auto &has_result_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	has_result = has_result_opt.HasResult();
	optional<TransactionModifierType> read_or_write {};
	auto &read_or_write_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (read_or_write_opt.HasResult()) {
		auto read_or_write_value = frame.TakeResult<TransactionModifierType>(child_slot);
		read_or_write = read_or_write_value;
		child_slot++;
	}
	auto result = TransformBeginTransaction(transformer, has_result, read_or_write);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeRollbackTransaction(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeRollbackTransaction
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	bool has_result {};
	auto &has_result_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	has_result = has_result_opt.HasResult();
	auto result = TransformRollbackTransaction(transformer, has_result);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeCommitTransaction(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCommitTransaction
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	bool has_result {};
	auto &has_result_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	has_result = has_result_opt.HasResult();
	auto result = TransformCommitTransaction(transformer, has_result);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeReadOrWrite(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(1), READ_ONLY_OR_READ_WRITE_OPS, parent_frame_index, child_slot);
	child_slot++;
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeReadOrWrite
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	idx_t child_slot = 0;
	auto read_only_or_read_write = frame.TakeResult<TransactionModifierType>(child_slot);
	child_slot++;
	auto result = TransformReadOrWrite(transformer, read_only_or_read_write);
	return make_uniq<TypedTransformResult<TransactionModifierType>>(result);
}

void PEGTransformerFactory::InitializeReadOnlyOrReadWrite(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeReadOnlyOrReadWrite(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<TransactionModifierType>(0);
	return make_uniq<TypedTransformResult<TransactionModifierType>>(result);
}

void PEGTransformerFactory::InitializeReadOnly(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeReadOnly
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformReadOnly(transformer);
	return make_uniq<TypedTransformResult<TransactionModifierType>>(result);
}

void PEGTransformerFactory::InitializeReadWrite(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeReadWrite
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformReadWrite(transformer);
	return make_uniq<TypedTransformResult<TransactionModifierType>>(result);
}

void PEGTransformerFactory::InitializeUseStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(1), USE_TARGET_OPS, parent_frame_index, child_slot);
	child_slot++;
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUseStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	idx_t child_slot = 0;
	auto use_target = frame.TakeResult<QualifiedName>(child_slot);
	child_slot++;
	auto result = TransformUseStatement(transformer, use_target);
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeUseTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUseTarget(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<QualifiedName>(0);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeSchemaNameAsUseTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeSchemaNameAsUseTarget
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto schema_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformSchemaNameAsUseTarget(transformer, schema_name);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeCatalogNameAsUseTarget(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCatalogNameAsUseTarget
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto catalog_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformCatalogNameAsUseTarget(transformer, catalog_name);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeUseTargetCatalogSchema(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &dot_identifier_opt_0 = list_pr.GetChild(3).Cast<OptionalParseResult>();
	if (dot_identifier_opt_0.HasResult()) {
		auto &dot_identifier_repeat_0 = dot_identifier_opt_0.GetResult().Cast<RepeatParseResult>();
		child_result_count += dot_identifier_repeat_0.GetChildren().size();
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (dot_identifier_opt_0.HasResult()) {
		auto &dot_identifier_repeat_0 = dot_identifier_opt_0.GetResult().Cast<RepeatParseResult>();
		auto dot_identifier_children_0 = dot_identifier_repeat_0.GetChildren();
		for (idx_t child_idx = dot_identifier_children_0.size(); child_idx > 0; child_idx--) {
			auto result_idx = child_idx - 1;
			stack.PushFrame(dot_identifier_children_0[result_idx].get(), DOT_IDENTIFIER_OPS, parent_frame_index, child_slot + result_idx);
		}
		child_slot += dot_identifier_children_0.size();
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUseTargetCatalogSchema
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	auto catalog_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto reserved_schema_name = list_pr.GetChild(2).Cast<IdentifierParseResult>().identifier;
	optional<vector<Identifier>> dot_identifier {};
	auto &dot_identifier_opt = list_pr.GetChild(3).Cast<OptionalParseResult>();
	if (dot_identifier_opt.HasResult()) {
		vector<Identifier> dot_identifier_value;
		auto &dot_identifier_repeat = dot_identifier_opt.GetResult().Cast<RepeatParseResult>();
		auto dot_identifier_children = dot_identifier_repeat.GetChildren();
		for (idx_t child_idx = 0; child_idx < dot_identifier_children.size(); child_idx++) {
			dot_identifier_value.push_back(frame.TakeResult<Identifier>(child_slot + child_idx));
		}
		dot_identifier = dot_identifier_value;
		child_slot += dot_identifier_children.size();
	}
	auto result = TransformUseTargetCatalogSchema(transformer, catalog_name, reserved_schema_name, dot_identifier);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeDotIdentifier(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDotIdentifier
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto identifier = list_pr.GetChild(1).Cast<IdentifierParseResult>().identifier;
	auto result = TransformDotIdentifier(transformer, identifier);
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

void PEGTransformerFactory::InitializeVacuumStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &vacuum_options_opt_0 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (vacuum_options_opt_0.HasResult()) {
		child_result_count++;
	}
	auto &analyze_target_opt_1 = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (analyze_target_opt_1.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (vacuum_options_opt_0.HasResult()) {
		stack.PushFrame(vacuum_options_opt_0.GetResult(), VACUUM_OPTIONS_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
	if (analyze_target_opt_1.HasResult()) {
		stack.PushFrame(analyze_target_opt_1.GetResult(), ANALYZE_TARGET_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeVacuumStatement
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	optional<VacuumOptions> vacuum_options {};
	auto &vacuum_options_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (vacuum_options_opt.HasResult()) {
		auto vacuum_options_value = frame.TakeResult<VacuumOptions>(child_slot);
		vacuum_options = vacuum_options_value;
		child_slot++;
	}
	optional<AnalyzeTarget> analyze_target {};
	auto &analyze_target_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (analyze_target_opt.HasResult()) {
		auto analyze_target_value = frame.TakeResult<AnalyzeTarget>(child_slot);
		analyze_target = std::move(analyze_target_value);
		child_slot++;
	}
	auto result = TransformVacuumStatement(transformer, vacuum_options, std::move(analyze_target));
	return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result));
}

void PEGTransformerFactory::InitializeVacuumOptions(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeVacuumOptions(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<VacuumOptions>(0);
	return make_uniq<TypedTransformResult<VacuumOptions>>(result);
}

void PEGTransformerFactory::InitializeVacuumParensOptions(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto vacuum_option_children_0 = ExtractParseResultsFromList(ExtractResultFromParens(list_pr.GetChild(0)));
	child_result_count += vacuum_option_children_0.size();
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	for (idx_t child_idx = vacuum_option_children_0.size(); child_idx > 0; child_idx--) {
		auto result_idx = child_idx - 1;
		stack.PushFrame(vacuum_option_children_0[result_idx].get(), VACUUM_OPTION_OPS, parent_frame_index, child_slot + result_idx);
	}
	child_slot += vacuum_option_children_0.size();
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeVacuumParensOptions
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	vector<string> vacuum_option;
	auto vacuum_option_items = ExtractParseResultsFromList(ExtractResultFromParens(list_pr.GetChild(0)));
	for (idx_t child_idx = 0; child_idx < vacuum_option_items.size(); child_idx++) {
		auto &vacuum_option_item = vacuum_option_items[child_idx];
		auto vacuum_option_child_value = frame.TakeResult<string>(child_slot + child_idx);
		vacuum_option.push_back(vacuum_option_child_value);
	}
	child_slot += vacuum_option_items.size();
	auto result = TransformVacuumParensOptions(transformer, vacuum_option);
	return make_uniq<TypedTransformResult<VacuumOptions>>(result);
}

void PEGTransformerFactory::InitializeVacuumLegacyOptions(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &opt_full_opt_0 = list_pr.GetChild(0).Cast<OptionalParseResult>();
	if (opt_full_opt_0.HasResult()) {
		child_result_count++;
	}
	auto &opt_freeze_opt_1 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (opt_freeze_opt_1.HasResult()) {
		child_result_count++;
	}
	auto &opt_verbose_opt_2 = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (opt_verbose_opt_2.HasResult()) {
		child_result_count++;
	}
	auto &opt_analyze_opt_3 = list_pr.GetChild(3).Cast<OptionalParseResult>();
	if (opt_analyze_opt_3.HasResult()) {
		child_result_count++;
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (opt_full_opt_0.HasResult()) {
		stack.PushFrame(opt_full_opt_0.GetResult(), OPT_FULL_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
	if (opt_freeze_opt_1.HasResult()) {
		stack.PushFrame(opt_freeze_opt_1.GetResult(), OPT_FREEZE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
	if (opt_verbose_opt_2.HasResult()) {
		stack.PushFrame(opt_verbose_opt_2.GetResult(), OPT_VERBOSE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
	if (opt_analyze_opt_3.HasResult()) {
		stack.PushFrame(opt_analyze_opt_3.GetResult(), OPT_ANALYZE_OPS, parent_frame_index, child_slot);
		child_slot++;
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeVacuumLegacyOptions
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	optional<string> opt_full {};
	auto &opt_full_opt = list_pr.GetChild(0).Cast<OptionalParseResult>();
	if (opt_full_opt.HasResult()) {
		auto opt_full_value = frame.TakeResult<string>(child_slot);
		opt_full = opt_full_value;
		child_slot++;
	}
	optional<string> opt_freeze {};
	auto &opt_freeze_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (opt_freeze_opt.HasResult()) {
		auto opt_freeze_value = frame.TakeResult<string>(child_slot);
		opt_freeze = opt_freeze_value;
		child_slot++;
	}
	optional<string> opt_verbose {};
	auto &opt_verbose_opt = list_pr.GetChild(2).Cast<OptionalParseResult>();
	if (opt_verbose_opt.HasResult()) {
		auto opt_verbose_value = frame.TakeResult<string>(child_slot);
		opt_verbose = opt_verbose_value;
		child_slot++;
	}
	optional<string> opt_analyze {};
	auto &opt_analyze_opt = list_pr.GetChild(3).Cast<OptionalParseResult>();
	if (opt_analyze_opt.HasResult()) {
		auto opt_analyze_value = frame.TakeResult<string>(child_slot);
		opt_analyze = opt_analyze_value;
		child_slot++;
	}
	auto result = TransformVacuumLegacyOptions(transformer, opt_full, opt_freeze, opt_verbose, opt_analyze);
	return make_uniq<TypedTransformResult<VacuumOptions>>(result);
}

void PEGTransformerFactory::InitializeVacuumOption(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeVacuumOption
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	string result;
	if (choice_result.type == ParseResultType::IDENTIFIER) {
		result = choice_result.Cast<IdentifierParseResult>().identifier.GetIdentifierName();
	} else if (choice_result.type == ParseResultType::KEYWORD) {
		result = choice_result.Cast<KeywordParseResult>().keyword;
	} else if (choice_result.type == ParseResultType::STRING) {
		result = choice_result.Cast<StringLiteralParseResult>().result;
	} else {
		result = frame.TakeResult<string>(0);
	}
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeOptAnalyze(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeOptAnalyze
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformOptAnalyze(transformer);
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeOptFull(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeOptFull
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformOptFull(transformer);
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeOptFreeze(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeOptFreeze
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformOptFreeze(transformer);
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeOptVerbose(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeOptVerbose
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformOptVerbose(transformer);
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeNameList(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto col_id_children_0 = ExtractParseResultsFromList(ExtractResultFromParens(list_pr.GetChild(0)));
	child_result_count += col_id_children_0.size();
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	for (idx_t child_idx = col_id_children_0.size(); child_idx > 0; child_idx--) {
		auto result_idx = child_idx - 1;
		stack.PushFrame(col_id_children_0[result_idx].get(), COL_ID_OPS, parent_frame_index, child_slot + result_idx);
	}
	child_slot += col_id_children_0.size();
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeNameList
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	vector<Identifier> col_id;
	auto col_id_items = ExtractParseResultsFromList(ExtractResultFromParens(list_pr.GetChild(0)));
	for (idx_t child_idx = 0; child_idx < col_id_items.size(); child_idx++) {
		auto &col_id_item = col_id_items[child_idx];
		auto col_id_child_value = frame.TakeResult<Identifier>(child_slot + child_idx);
		col_id.push_back(col_id_child_value);
	}
	child_slot += col_id_items.size();
	auto result = TransformNameList(transformer, col_id);
	return make_uniq<TypedTransformResult<vector<string>>>(result);
}

void PEGTransformerFactory::InitializeBaseTableName(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeBaseTableName(PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = frame.TakeResult<unique_ptr<BaseTableRef>>(0);
	return make_uniq<TypedTransformResult<unique_ptr<BaseTableRef>>>(std::move(result));
}

void PEGTransformerFactory::InitializeDottedIdentifier(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	auto &dot_col_label_opt_0 = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (dot_col_label_opt_0.HasResult()) {
		auto &dot_col_label_repeat_0 = dot_col_label_opt_0.GetResult().Cast<RepeatParseResult>();
		child_result_count += dot_col_label_repeat_0.GetChildren().size();
	}
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	if (dot_col_label_opt_0.HasResult()) {
		auto &dot_col_label_repeat_0 = dot_col_label_opt_0.GetResult().Cast<RepeatParseResult>();
		auto dot_col_label_children_0 = dot_col_label_repeat_0.GetChildren();
		for (idx_t child_idx = dot_col_label_children_0.size(); child_idx > 0; child_idx--) {
			auto result_idx = child_idx - 1;
			stack.PushFrame(dot_col_label_children_0[result_idx].get(), DOT_COL_LABEL_OPS, parent_frame_index, child_slot + result_idx);
		}
		child_slot += dot_col_label_children_0.size();
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDottedIdentifier
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	auto identifier = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	optional<vector<string>> dot_col_label {};
	auto &dot_col_label_opt = list_pr.GetChild(1).Cast<OptionalParseResult>();
	if (dot_col_label_opt.HasResult()) {
		vector<string> dot_col_label_value;
		auto &dot_col_label_repeat = dot_col_label_opt.GetResult().Cast<RepeatParseResult>();
		auto dot_col_label_children = dot_col_label_repeat.GetChildren();
		for (idx_t child_idx = 0; child_idx < dot_col_label_children.size(); child_idx++) {
			dot_col_label_value.push_back(frame.TakeResult<string>(child_slot + child_idx));
		}
		dot_col_label = dot_col_label_value;
		child_slot += dot_col_label_children.size();
	}
	auto result = TransformDottedIdentifier(transformer, identifier, dot_col_label);
	return make_uniq<TypedTransformResult<vector<string>>>(result);
}

void PEGTransformerFactory::InitializeNullLiteral(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeNullLiteral
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformNullLiteral(transformer);
	return make_uniq<TypedTransformResult<Value>>(result);
}

void PEGTransformerFactory::InitializeIfExists(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeIfExists
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformIfExists(transformer);
	return make_uniq<TypedTransformResult<bool>>(result);
}

void PEGTransformerFactory::InitializeColIdOrString(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	if (entry == trampoline_ops.end()) {
		frame.child_results.resize(0);
		return;
	}
	frame.child_results.resize(1);
	stack.PushFrame(choice_result, *entry->second, frame, 0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeColIdOrString
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	Identifier result;
	if (choice_result.type == ParseResultType::IDENTIFIER) {
		result = choice_result.Cast<IdentifierParseResult>().identifier;
	} else if (choice_result.type == ParseResultType::KEYWORD) {
		result = Identifier(choice_result.Cast<KeywordParseResult>().keyword);
	} else if (choice_result.type == ParseResultType::STRING) {
		result = Identifier(choice_result.Cast<StringLiteralParseResult>().result);
	} else {
		result = frame.TakeResult<Identifier>(0);
	}
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

void PEGTransformerFactory::InitializeIdentifierOrStringLiteral(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	frame.child_results.resize(0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeIdentifierOrStringLiteral
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	if (frame.child_results.empty()) {
		throw InternalException("Primitive choice rule IdentifierOrStringLiteral cannot produce QualifiedName");
	}
	auto result = frame.TakeResult<QualifiedName>(0);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeColId(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	frame.child_results.resize(0);
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeColId
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	auto &choice_result = choice_pr.GetResult();
	Identifier result;
	if (choice_result.type == ParseResultType::IDENTIFIER) {
		result = choice_result.Cast<IdentifierParseResult>().identifier;
	} else if (choice_result.type == ParseResultType::KEYWORD) {
		result = Identifier(choice_result.Cast<KeywordParseResult>().keyword);
	} else if (choice_result.type == ParseResultType::STRING) {
		result = Identifier(choice_result.Cast<StringLiteralParseResult>().result);
	} else {
		result = frame.TakeResult<Identifier>(0);
	}
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

void PEGTransformerFactory::InitializeCatalogReservedSchemaTable(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	child_result_count++;
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(0), CATALOG_QUALIFICATION_OPS, parent_frame_index, child_slot);
	child_slot++;
	stack.PushFrame(list_pr.GetChild(1), RESERVED_SCHEMA_QUALIFICATION_OPS, parent_frame_index, child_slot);
	child_slot++;
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCatalogReservedSchemaTable
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	auto catalog_qualification = frame.TakeResult<Identifier>(child_slot);
	child_slot++;
	auto reserved_schema_qualification = frame.TakeResult<Identifier>(child_slot);
	child_slot++;
	auto reserved_table_name = list_pr.GetChild(2).Cast<IdentifierParseResult>().identifier;
	auto result = TransformCatalogReservedSchemaTable(transformer, catalog_qualification, reserved_schema_qualification, reserved_table_name);
	return make_uniq<TypedTransformResult<unique_ptr<BaseTableRef>>>(std::move(result));
}

void PEGTransformerFactory::InitializeSchemaReservedTable(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_result_count = 0;
	child_result_count++;
	frame.child_results.resize(child_result_count);
	auto parent_frame_index = frame.frame_index;
	idx_t child_slot = 0;
	stack.PushFrame(list_pr.GetChild(0), SCHEMA_QUALIFICATION_OPS, parent_frame_index, child_slot);
	child_slot++;
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeSchemaReservedTable
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	idx_t child_slot = 0;
	auto schema_qualification = frame.TakeResult<Identifier>(child_slot);
	child_slot++;
	auto reserved_table_name = list_pr.GetChild(1).Cast<IdentifierParseResult>().identifier;
	auto result = TransformSchemaReservedTable(transformer, schema_qualification, reserved_table_name);
	return make_uniq<TypedTransformResult<unique_ptr<BaseTableRef>>>(std::move(result));
}

void PEGTransformerFactory::InitializeUnqualifiedBaseTableName(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUnqualifiedBaseTableName
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto table_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformUnqualifiedBaseTableName(transformer, table_name);
	return make_uniq<TypedTransformResult<unique_ptr<BaseTableRef>>>(std::move(result));
}

void PEGTransformerFactory::InitializeDotColLabel(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDotColLabel
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto col_label = TransformIdentifierOrKeyword(transformer, list_pr.GetChild(1));
	auto result = col_label;
	return make_uniq<TypedTransformResult<string>>(result);
}

void PEGTransformerFactory::InitializeCatalogQualification(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCatalogQualification
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto catalog_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformCatalogQualification(transformer, catalog_name);
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

void PEGTransformerFactory::InitializeReservedSchemaQualification(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeReservedSchemaQualification
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto reserved_schema_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = reserved_schema_name;
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

void PEGTransformerFactory::InitializeSchemaQualification(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeSchemaQualification
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto schema_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformSchemaQualification(transformer, schema_name);
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

const case_insensitive_map_t<const TransformFrameOps *> &PEGTransformerFactory::GeneratedTrampolineOps() {
	static const case_insensitive_map_t<const TransformFrameOps *> rule_ops = {
	    {"AnalyzeStatement", &ANALYZE_STATEMENT_OPS},
	    {"AnalyzeTarget", &ANALYZE_TARGET_OPS},
	    {"AnalyzeVerbose", &ANALYZE_VERBOSE_OPS},
	    {"CheckpointStatement", &CHECKPOINT_STATEMENT_OPS},
	    {"CheckpointForce", &CHECKPOINT_FORCE_OPS},
	    {"CommentStatement", &COMMENT_STATEMENT_OPS},
	    {"CommentOnType", &COMMENT_ON_TYPE_OPS},
	    {"CommentTable", &COMMENT_TABLE_OPS},
	    {"CommentSequence", &COMMENT_SEQUENCE_OPS},
	    {"CommentFunction", &COMMENT_FUNCTION_OPS},
	    {"CommentMacroTable", &COMMENT_MACRO_TABLE_OPS},
	    {"CommentMacro", &COMMENT_MACRO_OPS},
	    {"CommentView", &COMMENT_VIEW_OPS},
	    {"CommentDatabase", &COMMENT_DATABASE_OPS},
	    {"CommentIndex", &COMMENT_INDEX_OPS},
	    {"CommentSchema", &COMMENT_SCHEMA_OPS},
	    {"CommentType", &COMMENT_TYPE_OPS},
	    {"CommentColumn", &COMMENT_COLUMN_OPS},
	    {"CommentValue", &COMMENT_VALUE_OPS},
	    {"ConnectStatement", &CONNECT_STATEMENT_OPS},
	    {"DisconnectStatement", &DISCONNECT_STATEMENT_OPS},
	    {"SessionTarget", &SESSION_TARGET_OPS},
	    {"LocalSessionTarget", &LOCAL_SESSION_TARGET_OPS},
	    {"StringSessionTarget", &STRING_SESSION_TARGET_OPS},
	    {"CatalogSessionTarget", &CATALOG_SESSION_TARGET_OPS},
	    {"DeallocateStatement", &DEALLOCATE_STATEMENT_OPS},
	    {"DeallocatePrepare", &DEALLOCATE_PREPARE_OPS},
	    {"DetachStatement", &DETACH_STATEMENT_OPS},
	    {"LoadStatement", &LOAD_STATEMENT_OPS},
	    {"ExtensionAlias", &EXTENSION_ALIAS_OPS},
	    {"InstallStatement", &INSTALL_STATEMENT_OPS},
	    {"UpdateExtensionsStatement", &UPDATE_EXTENSIONS_STATEMENT_OPS},
	    {"FromSource", &FROM_SOURCE_OPS},
	    {"FromSourceIdentifier", &FROM_SOURCE_IDENTIFIER_OPS},
	    {"FromSourceString", &FROM_SOURCE_STRING_OPS},
	    {"VersionNumber", &VERSION_NUMBER_OPS},
	    {"TransactionStatement", &TRANSACTION_STATEMENT_OPS},
	    {"BeginTransaction", &BEGIN_TRANSACTION_OPS},
	    {"RollbackTransaction", &ROLLBACK_TRANSACTION_OPS},
	    {"CommitTransaction", &COMMIT_TRANSACTION_OPS},
	    {"ReadOrWrite", &READ_OR_WRITE_OPS},
	    {"ReadOnlyOrReadWrite", &READ_ONLY_OR_READ_WRITE_OPS},
	    {"ReadOnly", &READ_ONLY_OPS},
	    {"ReadWrite", &READ_WRITE_OPS},
	    {"UseStatement", &USE_STATEMENT_OPS},
	    {"UseTarget", &USE_TARGET_OPS},
	    {"SchemaNameAsUseTarget", &SCHEMA_NAME_AS_USE_TARGET_OPS},
	    {"CatalogNameAsUseTarget", &CATALOG_NAME_AS_USE_TARGET_OPS},
	    {"UseTargetCatalogSchema", &USE_TARGET_CATALOG_SCHEMA_OPS},
	    {"DotIdentifier", &DOT_IDENTIFIER_OPS},
	    {"VacuumStatement", &VACUUM_STATEMENT_OPS},
	    {"VacuumOptions", &VACUUM_OPTIONS_OPS},
	    {"VacuumParensOptions", &VACUUM_PARENS_OPTIONS_OPS},
	    {"VacuumLegacyOptions", &VACUUM_LEGACY_OPTIONS_OPS},
	    {"VacuumOption", &VACUUM_OPTION_OPS},
	    {"OptAnalyze", &OPT_ANALYZE_OPS},
	    {"OptFull", &OPT_FULL_OPS},
	    {"OptFreeze", &OPT_FREEZE_OPS},
	    {"OptVerbose", &OPT_VERBOSE_OPS},
	    {"NameList", &NAME_LIST_OPS},
	    {"BaseTableName", &BASE_TABLE_NAME_OPS},
	    {"DottedIdentifier", &DOTTED_IDENTIFIER_OPS},
	    {"NullLiteral", &NULL_LITERAL_OPS},
	    {"IfExists", &IF_EXISTS_OPS},
	    {"ColIdOrString", &COL_ID_OR_STRING_OPS},
	    {"IdentifierOrStringLiteral", &IDENTIFIER_OR_STRING_LITERAL_OPS},
	    {"ColId", &COL_ID_OPS},
	    {"CatalogReservedSchemaTable", &CATALOG_RESERVED_SCHEMA_TABLE_OPS},
	    {"SchemaReservedTable", &SCHEMA_RESERVED_TABLE_OPS},
	    {"UnqualifiedBaseTableName", &UNQUALIFIED_BASE_TABLE_NAME_OPS},
	    {"DotColLabel", &DOT_COL_LABEL_OPS},
	    {"CatalogQualification", &CATALOG_QUALIFICATION_OPS},
	    {"ReservedSchemaQualification", &RESERVED_SCHEMA_QUALIFICATION_OPS},
	    {"SchemaQualification", &SCHEMA_QUALIFICATION_OPS},
	};
	return rule_ops;
}
// END generated trampoline transformer rules

void PEGTransformerFactory::RegisterGeneratedTrampoline() {
	trampoline_transform_functions["Statement"] = &PEGTransformerFactory::TransformStatementTrampolineInternal;
}

} // namespace duckdb
