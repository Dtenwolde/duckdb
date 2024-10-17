#include <duckdb/parser/peg_transformer.hpp>
#include <duckdb/parser/statement/transaction_statement.hpp>
#include <duckdb/parser/expression/columnref_expression.hpp>

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
    } else if (ast->name == "SelectStatement") {
        return TransformSelect(ast);
    } else {
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }
}

TransactionType TransformTransactionType(const std::string_view kind) {
    if (kind == "BEGIN TRANSACTION") {
        return TransactionType::BEGIN_TRANSACTION;
    } else if (kind == "COMMIT") {
        return TransactionType::COMMIT;
    } else if (kind == "ROLLBACK") {
        return TransactionType::ROLLBACK;
    } else {
        throw NotImplementedException("Transaction type " + string(kind) + " not implemented yet.");
    }
}

unique_ptr<TransactionStatement> PEGTransformer::TransformTransaction(std::shared_ptr<peg::Ast> &ast) {
    auto type = TransformTransactionType(ast->nodes[0]->token);
    auto info = make_uniq<TransactionInfo>(type);
    return make_uniq<TransactionStatement>(std::move(info));
}

unique_ptr<ParsedExpression> PEGTransformer::TransformExpression(std::shared_ptr<peg::Ast> &ast) {
    for (auto &child : ast->nodes) {
        if (child->name == "SingleExpression") {

        }
    }
    throw NotImplementedException("Transform for " + ast->name + " not implemented");
}

string PEGTransformer::TransformIdentifier(std::shared_ptr<peg::Ast> &ast) {
    return string(ast->nodes[0]->token);
}

unique_ptr<ParsedExpression> PEGTransformer::TransformAliasedExpression(std::vector<std::shared_ptr<peg::Ast>> &aliased_expr_nodes){
    if (aliased_expr_nodes.size() != 2) {
        throw ParserException("AliasedExpression node should have 2 children");
    }
    if (aliased_expr_nodes[0]->name == "Expression") {
        auto column_name = TransformExpression(aliased_expr_nodes[0]);
    } else {
        throw NotImplementedException("Not implemented yet.");
    }
    if (aliased_expr_nodes[1]->name == "Identifier") {
        auto alias = TransformIdentifier(aliased_expr_nodes[1]);
    } else {
        throw ParserException("AliasedExpression node should have Identifier as second child");
    }
//    auto result = make_uniq<ColumnRefExpression>(column_name);
//    result->alias = alias;
//    return result;
    throw NotImplementedException("Not implemented yet.");
}

void PEGTransformer::TransformSelectList(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<ParsedExpression>> &select_list) {
    for (auto &child : ast->nodes) {
        if (child->name == "AliasedExpression") {
            auto expr = TransformAliasedExpression(child->nodes);
            select_list.push_back(move(expr));
        }
    }
}

unique_ptr<TableRef> PEGTransformer::TransformFrom(std::shared_ptr<peg::Ast> &ast) {
    throw NotImplementedException("Transform for " + ast->name + " not implemented");
}

unique_ptr<ParsedExpression> PEGTransformer::TransformWhere(std::shared_ptr<peg::Ast> &ast) {
    throw NotImplementedException("Transform for " + ast->name + " not implemented");
}

GroupByNode PEGTransformer::TransformGroupBy(std::shared_ptr<peg::Ast> &ast) {
    throw NotImplementedException("Transform for " + ast->name + " not implemented");
}

unique_ptr<SelectStatement> PEGTransformer::TransformSelect(std::shared_ptr<peg::Ast> &ast) {
    for (auto &child : ast->nodes) {
        if (child->name == "SimpleSelect") {
            auto result = make_uniq<SelectStatement>();
            auto select_node = make_uniq<SelectNode>();
            for (auto &child2 : child->nodes) {
                if (child2->name == "SelectClause") {
                    vector<unique_ptr<ParsedExpression>> select_list;
                    TransformSelectList(child2, select_list);
                    select_node->select_list = std::move(select_list);
                } else if (child2->name == "FromClause") {
                    select_node->from_table = TransformFrom(child2);
                } else if (child2->name == "WhereClause") {
                    select_node->where_clause = TransformWhere(child2);
                } else if (child2->name == "GroupByClause") {
                    select_node->groups = TransformGroupBy(child2);
                }
            }
        }
    }

    throw NotImplementedException("Transform for " + ast->name + " not implemented");
}


} // namespace duckdb
