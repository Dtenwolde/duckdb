#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/statement/transaction_statement.hpp"

namespace duckdb {

// TransactionStatement <- BeginTransaction / RollbackTransaction / CommitTransaction
void PEGTransformerFactory::T_TransformTransactionStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	t.PushFrame(choice_pr.GetResult(), frame);
	frame.state = TransformState::WAITING;
}

void PEGTransformerFactory::R_TransformTransactionStatement(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	const auto &chosen_name = choice_pr.GetResult().name;
	frame.SetParentResult(std::move(frame.child_results[chosen_name]));
	t.PopFrame();
}

// BeginTransaction <- StartOrBegin Transaction? ReadOrWrite?
void PEGTransformerFactory::T_TransformBeginTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	auto &rw_opt = list_pr.Child<OptionalParseResult>(2);
	if (rw_opt.HasResult()) {
		t.PushFrame(rw_opt.GetResult(), frame);
	}
	frame.state = TransformState::WAITING;
}

void PEGTransformerFactory::R_TransformBeginTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	auto info = make_uniq<TransactionInfo>(TransactionType::BEGIN_TRANSACTION);
	info->modifier = frame.GetOptionalResult("ReadOrWrite", TransactionModifierType::TRANSACTION_DEFAULT_MODIFIER);
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(make_uniq<TransactionStatement>(std::move(info))));
	t.PopFrame();
}

// CommitTransaction <- CommitOrEnd Transaction?
void PEGTransformerFactory::T_TransformCommitTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	frame.state = TransformState::WAITING;
}

void PEGTransformerFactory::R_TransformCommitTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(
	    make_uniq<TransactionStatement>(make_uniq<TransactionInfo>(TransactionType::COMMIT))));
	t.PopFrame();
}

// RollbackTransaction <- AbortOrRollback Transaction?
void PEGTransformerFactory::T_TransformRollbackTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	frame.state = TransformState::WAITING;
}

void PEGTransformerFactory::R_TransformRollbackTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	frame.SetParentResult(MakeResult<unique_ptr<SQLStatement>>(
	    make_uniq<TransactionStatement>(make_uniq<TransactionInfo>(TransactionType::ROLLBACK))));
	t.PopFrame();
}

// ReadOrWrite <- 'READ' ReadOnlyOrReadWrite
void PEGTransformerFactory::T_TransformReadOrWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	t.PushFrame(list_pr.GetChild(1), frame);
	frame.state = TransformState::WAITING;
}

void PEGTransformerFactory::R_TransformReadOrWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto modifier = CastResult<TransactionModifierType>(std::move(frame.child_results["ReadOnlyOrReadWrite"]));
	frame.SetParentResult(MakeResult<TransactionModifierType>(modifier));
	t.PopFrame();
}

// ReadOnlyOrReadWrite <- ReadOnly / ReadWrite  (leaf -- resolved in Init, never enters WAITING)
void PEGTransformerFactory::T_TransformReadOnlyOrReadWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &choice_pr = frame.parse_result->Cast<ListParseResult>().Child<ChoiceParseResult>(0);
	auto modifier = (choice_pr.GetResult().name == "ReadOnly") ? TransactionModifierType::TRANSACTION_READ_ONLY
	                                                           : TransactionModifierType::TRANSACTION_READ_WRITE;
	frame.SetParentResult(MakeResult<TransactionModifierType>(modifier));
	t.PopFrame();
}

} // namespace duckdb