#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/statement/transaction_statement.hpp"

namespace duckdb {

// BeginTransaction <- StartOrBegin Transaction? ReadOrWrite?
void PEGTransformerFactory::T_TransformBeginTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	auto &read_or_write = list_pr.Child<OptionalParseResult>(2);

	if (!frame.child_result) {
		if (!read_or_write.HasResult()) {
			// Leaf: plain BEGIN/START with no modifier
			auto info = make_uniq<TransactionInfo>(TransactionType::BEGIN_TRANSACTION);
			info->modifier = TransactionModifierType::TRANSACTION_DEFAULT_MODIFIER;
			t.SetParentResult(t.MakeStatementResult(make_uniq<TransactionStatement>(std::move(info))));
			t.PopFrame();
			return;
		}
		// Push ReadOrWrite to resolve the modifier
		t.PushFrame(read_or_write.GetResult());
		return;
	}
	// Second visit: got modifier from ReadOrWrite
	auto modifier = CastResult<TransactionModifierType>(std::move(frame.child_result));
	auto info = make_uniq<TransactionInfo>(TransactionType::BEGIN_TRANSACTION);
	info->modifier = modifier;
	t.SetParentResult(t.MakeStatementResult(make_uniq<TransactionStatement>(std::move(info))));
	t.PopFrame();
}

// CommitTransaction <- CommitOrEnd Transaction?
void PEGTransformerFactory::T_TransformCommitTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	t.SetParentResult(
	    t.MakeStatementResult(make_uniq<TransactionStatement>(make_uniq<TransactionInfo>(TransactionType::COMMIT))));
	t.PopFrame();
}

// RollbackTransaction <- AbortOrRollback Transaction?
void PEGTransformerFactory::T_TransformRollbackTransaction(PEGTransformer &t, TransformerStackFrame &frame) {
	t.SetParentResult(
	    t.MakeStatementResult(make_uniq<TransactionStatement>(make_uniq<TransactionInfo>(TransactionType::ROLLBACK))));
	t.PopFrame();
}

// ReadOnlyOrReadWrite <- ReadOnly / ReadWrite  (enum dispatch -- no child Transform calls)
void PEGTransformerFactory::T_TransformReadOnlyOrReadWrite(PEGTransformer &t, TransformerStackFrame &frame) {
	auto &list_pr = frame.parse_result->Cast<ListParseResult>();
	auto modifier = t.TransformEnum<TransactionModifierType>(list_pr.Child<ChoiceParseResult>(0).GetResult());
	t.SetParentResult(MakeResult<TransactionModifierType>(modifier));
	t.PopFrame();
}

} // namespace duckdb