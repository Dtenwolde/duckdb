#include "duckdb/parser/peg/transformer/peg_transformer.hpp"

namespace duckdb {

// BEGIN generated trampoline transformer rules
const TransformFrameOps PEGTransformerFactory::CHECKPOINT_STATEMENT_OPS = {
    "CheckpointStatement", &PEGTransformerFactory::InitializeCheckpointStatement,
    &PEGTransformerFactory::FinalizeCheckpointStatement};
const TransformFrameOps PEGTransformerFactory::CHECKPOINT_FORCE_OPS = {
    "CheckpointForce", &PEGTransformerFactory::InitializeCheckpointForce,
    &PEGTransformerFactory::FinalizeCheckpointForce};
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
const TransformFrameOps PEGTransformerFactory::IF_EXISTS_OPS = {
    "IfExists", &PEGTransformerFactory::InitializeIfExists,
    &PEGTransformerFactory::FinalizeIfExists};

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
	frame.child_results.resize(1);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	D_ASSERT(entry != trampoline_ops.end());
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
	auto string_literal = list_pr.GetChild(0).Cast<StringLiteralParseResult>().GetRawString();
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

void PEGTransformerFactory::InitializeTransactionStatement(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	frame.child_results.resize(1);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	D_ASSERT(entry != trampoline_ops.end());
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
	frame.child_results.resize(1);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	D_ASSERT(entry != trampoline_ops.end());
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
	frame.child_results.resize(1);
	auto &choice_result = choice_pr.GetResult();
	auto &trampoline_ops = GeneratedTrampolineOps();
	auto entry = trampoline_ops.find(choice_result.name);
	D_ASSERT(entry != trampoline_ops.end());
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

void PEGTransformerFactory::InitializeIfExists(PEGTransformer &transformer, TransformStack &stack,
                                                TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeIfExists
    (PEGTransformer &transformer, TransformStackFrame &frame) {
	auto result = TransformIfExists(transformer);
	return make_uniq<TypedTransformResult<bool>>(result);
}

const case_insensitive_map_t<const TransformFrameOps *> &PEGTransformerFactory::GeneratedTrampolineOps() {
	static const case_insensitive_map_t<const TransformFrameOps *> rule_ops = {
	    {"CheckpointStatement", &CHECKPOINT_STATEMENT_OPS},
	    {"CheckpointForce", &CHECKPOINT_FORCE_OPS},
	    {"ConnectStatement", &CONNECT_STATEMENT_OPS},
	    {"DisconnectStatement", &DISCONNECT_STATEMENT_OPS},
	    {"SessionTarget", &SESSION_TARGET_OPS},
	    {"LocalSessionTarget", &LOCAL_SESSION_TARGET_OPS},
	    {"StringSessionTarget", &STRING_SESSION_TARGET_OPS},
	    {"CatalogSessionTarget", &CATALOG_SESSION_TARGET_OPS},
	    {"DeallocateStatement", &DEALLOCATE_STATEMENT_OPS},
	    {"DeallocatePrepare", &DEALLOCATE_PREPARE_OPS},
	    {"DetachStatement", &DETACH_STATEMENT_OPS},
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
	    {"IfExists", &IF_EXISTS_OPS},
	};
	return rule_ops;
}
// END generated trampoline transformer rules

void PEGTransformerFactory::RegisterGeneratedTrampoline() {
	trampoline_transform_functions["Statement"] = &PEGTransformerFactory::TransformStatementTrampolineInternal;
}

} // namespace duckdb
