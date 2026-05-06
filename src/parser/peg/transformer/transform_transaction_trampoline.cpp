#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/statement/transaction_statement.hpp"

namespace duckdb {

// TransactionStatement <- BeginTransaction / RollbackTransaction / CommitTransaction
void PEGTransformerFactory::T_TransformTransactionStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	t.PushChoiceFrame(frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0), frame);
}

void PEGTransformerFactory::R_TransformTransactionStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	frame.SetParentResult(frame.GetChoiceResult(choice_pr));
}

// BeginTransaction <- StartOrBegin Transaction? ReadOrWrite?
void PEGTransformerFactory::T_TransformBeginTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	t.PushOptionalFrame(list_pr.Child<OptionalParseResult>(2), frame);
}

void PEGTransformerFactory::R_TransformBeginTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	auto info = make_uniq<TransactionInfo>(TransactionType::BEGIN_TRANSACTION);
	info->modifier = frame.GetOptionalResult("ReadOrWrite", TransactionModifierType::TRANSACTION_DEFAULT_MODIFIER);
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(make_uniq<TransactionStatement>(std::move(info))));
}

// CommitTransaction <- CommitOrEnd Transaction?
void PEGTransformerFactory::T_TransformCommitTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
}

void PEGTransformerFactory::R_TransformCommitTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(
	    make_uniq<TransactionStatement>(make_uniq<TransactionInfo>(TransactionType::COMMIT))));
}

// RollbackTransaction <- AbortOrRollback Transaction?
void PEGTransformerFactory::T_TransformRollbackTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
}

void PEGTransformerFactory::R_TransformRollbackTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(
	    make_uniq<TransactionStatement>(make_uniq<TransactionInfo>(TransactionType::ROLLBACK))));
}

// ReadOrWrite <- 'READ' ReadOnlyOrReadWrite
void PEGTransformerFactory::T_TransformReadOrWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	t.PushFrame(list_pr.GetChild(1), frame);
}

void PEGTransformerFactory::R_TransformReadOrWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto modifier = CastResult<TransactionModifierType>(std::move(frame.child_results["ReadOnlyOrReadWrite"]));
	frame.SetParentResult(MakeResult<TransactionModifierType>(modifier));
}

// ReadOnlyOrReadWrite <- ReadOnly / ReadWrite  (leaf -- no children to push)
void PEGTransformerFactory::T_TransformReadOnlyOrReadWrite(PEGTransformer &t, TransformerStackFrame &frame) {
}

void PEGTransformerFactory::R_TransformReadOnlyOrReadWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	auto modifier = (choice_pr.GetResult().name == "ReadOnly") ? TransactionModifierType::TRANSACTION_READ_ONLY
	                                                           : TransactionModifierType::TRANSACTION_READ_WRITE;
	frame.SetParentResult(MakeResult<TransactionModifierType>(modifier));
}

} // namespace duckdb
