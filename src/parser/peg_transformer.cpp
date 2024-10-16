#include <duckdb/parser/peg_transformer.hpp>
#include <duckdb/parser/statement/transaction_statement.hpp>

namespace duckdb {

PEGTransformer::PEGTransformer(ParserOptions &options)
        : parent(nullptr), options(options), stack_depth(DConstants::INVALID_INDEX) {
}

void PEGTransformer::Transform(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<SQLStatement>> &statements) {
    if (ast->name == "SingleStatement") {
        for (auto &child : ast->nodes) {
            statements.push_back(TransformSingleStatement(child));
        }
    } else {
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }
}

unique_ptr<SQLStatement> PEGTransformer::TransformSingleStatement(std::shared_ptr<peg::Ast> &ast) {
    if (ast->name == "TransactionStatement") {
        return TransformTransaction(ast);
    } else {
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }
}

TransactionType TransformTransactionType(const std::string_view kind) {
    if (kind == "BEGIN") {
        return TransactionType::BEGIN_TRANSACTION;
    } else if (kind == "COMMIT") {
        return TransactionType::COMMIT;
    } else if (kind == "ROLLBACK") {
        return TransactionType::ROLLBACK;
    } else {
        throw NotImplementedException("Transaction type not implemented yet.");
    }
}

unique_ptr<TransactionStatement> PEGTransformer::TransformTransaction(std::shared_ptr<peg::Ast> &ast) {
    auto type = TransformTransactionType(ast->nodes[0]->token);
    auto info = make_uniq<TransactionInfo>(type);
    return make_uniq<TransactionStatement>(std::move(info));
}



} // namespace duckdb
