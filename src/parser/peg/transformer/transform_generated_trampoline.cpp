#include "duckdb/parser/peg/transformer/peg_transformer.hpp"

namespace duckdb {

// BEGIN generated trampoline transformer rules
const TransformFrameOps PEGTransformerFactory::CHECKPOINT_STATEMENT_OPS = {
    "CheckpointStatement", &PEGTransformerFactory::InitializeCheckpointStatement,
    &PEGTransformerFactory::FinalizeCheckpointStatement};
const TransformFrameOps PEGTransformerFactory::CHECKPOINT_FORCE_OPS = {
    "CheckpointForce", &PEGTransformerFactory::InitializeCheckpointForce,
    &PEGTransformerFactory::FinalizeCheckpointForce};

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

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCheckpointStatement(PEGTransformer &transformer,
                                                                                    TransformStackFrame &frame) {
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

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCheckpointForce(PEGTransformer &transformer,
                                                                                TransformStackFrame &frame) {
	auto result = TransformCheckpointForce(transformer);
	return make_uniq<TypedTransformResult<bool>>(result);
}

const TransformFrameOps PEGTransformerFactory::USE_STATEMENT_OPS = {
    "UseStatement", &PEGTransformerFactory::InitializeUseStatement, &PEGTransformerFactory::FinalizeUseStatement};
const TransformFrameOps PEGTransformerFactory::USE_TARGET_OPS = {
    "UseTarget", &PEGTransformerFactory::InitializeUseTarget, &PEGTransformerFactory::FinalizeUseTarget};
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
    "DotIdentifier", &PEGTransformerFactory::InitializeDotIdentifier, &PEGTransformerFactory::FinalizeDotIdentifier};

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

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUseStatement(PEGTransformer &transformer,
                                                                             TransformStackFrame &frame) {
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

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUseTarget(PEGTransformer &transformer,
                                                                          TransformStackFrame &frame) {
	auto result = frame.TakeResult<QualifiedName>(0);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeSchemaNameAsUseTarget(PEGTransformer &transformer, TransformStack &stack,
                                                            TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeSchemaNameAsUseTarget(PEGTransformer &transformer,
                                                                                      TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto schema_name = list_pr.GetChild(0).Cast<IdentifierParseResult>().identifier;
	auto result = TransformSchemaNameAsUseTarget(transformer, schema_name);
	return make_uniq<TypedTransformResult<QualifiedName>>(result);
}

void PEGTransformerFactory::InitializeCatalogNameAsUseTarget(PEGTransformer &transformer, TransformStack &stack,
                                                             TransformStackFrame &frame) {
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeCatalogNameAsUseTarget(PEGTransformer &transformer,
                                                                                       TransformStackFrame &frame) {
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
			stack.PushFrame(dot_identifier_children_0[result_idx].get(), DOT_IDENTIFIER_OPS, parent_frame_index,
			                child_slot + result_idx);
		}
		child_slot += dot_identifier_children_0.size();
	}
}

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeUseTargetCatalogSchema(PEGTransformer &transformer,
                                                                                       TransformStackFrame &frame) {
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

unique_ptr<TransformResultValue> PEGTransformerFactory::FinalizeDotIdentifier(PEGTransformer &transformer,
                                                                              TransformStackFrame &frame) {
	auto &list_pr = frame.parse_result.Cast<ListParseResult>();
	auto identifier = list_pr.GetChild(1).Cast<IdentifierParseResult>().identifier;
	auto result = TransformDotIdentifier(transformer, identifier);
	return make_uniq<TypedTransformResult<Identifier>>(result);
}

const case_insensitive_map_t<const TransformFrameOps *> &PEGTransformerFactory::GeneratedTrampolineOps() {
	static const case_insensitive_map_t<const TransformFrameOps *> rule_ops = {
	    {"CheckpointStatement", &CHECKPOINT_STATEMENT_OPS},
	    {"CheckpointForce", &CHECKPOINT_FORCE_OPS},
	    {"UseStatement", &USE_STATEMENT_OPS},
	    {"UseTarget", &USE_TARGET_OPS},
	    {"SchemaNameAsUseTarget", &SCHEMA_NAME_AS_USE_TARGET_OPS},
	    {"CatalogNameAsUseTarget", &CATALOG_NAME_AS_USE_TARGET_OPS},
	    {"UseTargetCatalogSchema", &USE_TARGET_CATALOG_SCHEMA_OPS},
	    {"DotIdentifier", &DOT_IDENTIFIER_OPS},
	};
	return rule_ops;
}
// END generated trampoline transformer rules

void PEGTransformerFactory::RegisterGeneratedTrampoline() {
	trampoline_transform_functions["Statement"] = &PEGTransformerFactory::TransformStatementTrampolineInternal;
}

} // namespace duckdb
