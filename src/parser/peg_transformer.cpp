#include <duckdb/parser/peg_transformer.hpp>
#include <duckdb/parser/statement/transaction_statement.hpp>
#include <duckdb/parser/expression/columnref_expression.hpp>
#include <duckdb/parser/statement/create_statement.hpp>
#include <duckdb/parser/parsed_data/create_table_info.hpp>
#include <duckdb/common/enums/catalog_type.hpp>
#include <duckdb/parser/tableref/basetableref.hpp>

namespace duckdb {

    PEGTransformer::PEGTransformer(ParserOptions &options)
            : parent(nullptr), options(options), stack_depth(DConstants::INVALID_INDEX) {
    }

    void PEGTransformer::Transform(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<SQLStatement>> &statements) {
        if (ast->name == "SingleStatement") {
            for (auto &child: ast->nodes) {
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
        } else if (ast->name == "CreateStatement") {
            return TransformCreateTable(ast);
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

    void PEGTransformer::TransformColumnDefinition(std::shared_ptr<peg::Ast> &ast, ColumnList &column_list) {
        if (ast->nodes.size() != 2) {
            throw ParserException("ColumnDefinition node should have 2 children");
        }
        auto column_name = TransformIdentifier(ast->nodes[0]);
        if (ast->nodes[1]->name == "TypeSpecifier") {
            auto type = TransformIdentifier(ast->nodes[1]->nodes[0]);
            auto column_type = EnumUtil::FromString<LogicalTypeId>(duckdb::StringUtil::Upper(type));
            column_list.AddColumn(ColumnDefinition(column_name, column_type));
        } else {
            throw ParserException("ColumnDefinition node should have TypeSpecifier as second child");
        }
    }

    unique_ptr<CreateStatement> PEGTransformer::TransformCreateTable(std::shared_ptr<peg::Ast> &ast) {
        auto result = make_uniq<CreateStatement>();
        auto table_info = make_uniq<CreateTableInfo>();
        if (ast->nodes[0]->name == "Identifier") {
             table_info->table = TransformIdentifier(ast->nodes[0]);
        } else {
            throw ParserException("CreateStatement node should have Identifier as first child");
        }
        if (ast->nodes[1]->name == "ColumnDefinition") {
            ColumnList columns;
            TransformColumnDefinition(ast->nodes[1], columns);
            table_info->columns = std::move(columns);
        }
        result->info = std::move(table_info);
        return result;
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformLiteralExpression(std::shared_ptr<peg::Ast> &ast) {
        auto expr_child = ast->nodes[0];
        if (expr_child->name == "StringLiteral") {
            return make_uniq<ConstantExpression>(Value(string(expr_child->token)));
        } else if (expr_child->name == "NumberLiteral") {
            return make_uniq<ConstantExpression>(Value(stoi(string(expr_child->token))));
        } else {
            throw NotImplementedException("Transform for " + expr_child->name + " not implemented");
        }
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformColumnReference(std::shared_ptr<peg::Ast> &ast) {
        auto col_name = TransformIdentifier(ast->nodes[0]);
        if (ast->nodes.size() == 2) {
            auto table_name = TransformIdentifier(ast->nodes[1]);
            auto result = make_uniq<ColumnRefExpression>(col_name, table_name);
            return result;
        }
        return make_uniq<ColumnRefExpression>(col_name);
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformSingleExpression(std::shared_ptr<peg::Ast> &ast) {
        auto expr_child = ast->nodes[0];
        if (expr_child->name == "SubqueryExpression") {
            throw NotImplementedException("SubqueryExpression not implemented yet.");
        } else if (expr_child->name == "LiteralListExpression") {
            throw NotImplementedException("LiteralListExpression not implemented yet.");
        } else if (expr_child->name == "ParenthesisExpression") {
            throw NotImplementedException("ParenthesisExpression not implemented yet.");
        } else if (expr_child->name == "DateExpression") {
            throw NotImplementedException("DateExpression not implemented yet.");
        } else if (expr_child->name == "DistinctExpression") {
            throw NotImplementedException("DistinctExpression not implemented yet.");
        } else if (expr_child->name == "SubstringExpression") {
            throw NotImplementedException("SubstringExpression not implemented yet.");
        } else if (expr_child->name == "IsNullExpression") {
            throw NotImplementedException("IsNullExpression not implemented yet.");
        } else if (expr_child->name == "CaseExpression") {
            throw NotImplementedException("CaseExpression not implemented yet.");
        } else if (expr_child->name == "CountStarExpression") {
            throw NotImplementedException("CountStarExpression not implemented yet.");
        } else if (expr_child->name == "CastExpression") {
            throw NotImplementedException("CastExpression not implemented yet.");
        } else if (expr_child->name == "ExtractExpression") {
            throw NotImplementedException("ExtractExpression not implemented yet.");
        } else if (expr_child->name == "WindowExpression") {
            throw NotImplementedException("WindowExpression not implemented yet.");
        } else if (expr_child->name == "FunctionExpression") {
            throw NotImplementedException("FunctionExpression not implemented yet.");
        } else if (expr_child->name == "ColumnReference") {
            return TransformColumnReference(expr_child);
        } else if (expr_child->name == "LiteralExpression") {
            return TransformLiteralExpression(expr_child);
        } else {
            throw NotImplementedException("Transform for " + ast->name + " not implemented");
        }
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformExpression(std::shared_ptr<peg::Ast> &ast) {
        for (auto &child: ast->nodes) {
            if (child->name == "SingleExpression") {
                return TransformSingleExpression(child);
            }
        }
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }

    string PEGTransformer::TransformIdentifier(std::shared_ptr<peg::Ast> &ast) {
        return string(ast->nodes[0]->token);
    }

    unique_ptr<ParsedExpression>
    PEGTransformer::TransformAliasedExpression(std::vector<std::shared_ptr<peg::Ast>> &aliased_expr_nodes) {
        unique_ptr<ParsedExpression> expr;
        string alias;

        if (aliased_expr_nodes[0]->name == "Expression") {
            expr = TransformExpression(aliased_expr_nodes[0]);
        } else {
            throw NotImplementedException("Not implemented yet.");
        }
        if (aliased_expr_nodes.size() == 2) {
            if (aliased_expr_nodes[1]->name == "Identifier") {
                alias = TransformIdentifier(aliased_expr_nodes[1]);
            } else {
                throw ParserException("AliasedExpression node should have Identifier as second child");
            }
            if (!expr) {
                throw ParserException("AliasedExpression node should have Expression as first child");
            }
            expr->alias = alias;
        }

        return expr;
    }

    void PEGTransformer::TransformSelectList(std::shared_ptr<peg::Ast> &ast,
                                             vector<unique_ptr<ParsedExpression>> &select_list) {
        for (auto &child: ast->nodes) {
            if (child->name == "AliasedExpression") {
                select_list.push_back(TransformAliasedExpression(child->nodes));
            }
        }
    }

    unique_ptr<TableRef> PEGTransformer::TransformTableReference(std::shared_ptr<peg::Ast> &ast) {
        if (ast->nodes[0]->name == "Identifier") {
            auto base_table_ref = make_uniq<BaseTableRef>();
            base_table_ref->table_name = TransformIdentifier(ast->nodes[0]);
            return base_table_ref;
        }
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }



    unique_ptr<TableRef> PEGTransformer::TransformFrom(std::shared_ptr<peg::Ast> &ast) {
        auto table_ref = TransformTableReference(ast->nodes[0]);
        if (ast->nodes.size() == 1){
            return table_ref;
        }
        throw NotImplementedException("Transform for " + ast->name + " not implemented");

    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformWhere(std::shared_ptr<peg::Ast> &ast) {
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }

    GroupByNode PEGTransformer::TransformGroupBy(std::shared_ptr<peg::Ast> &ast) {
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }

    unique_ptr<SelectStatement> PEGTransformer::TransformSelect(std::shared_ptr<peg::Ast> &ast) {
        for (auto &child: ast->nodes) {
            if (child->name == "SimpleSelect") {
                auto result = make_uniq<SelectStatement>();
                auto select_node = make_uniq<SelectNode>();
                select_node->from_table = make_uniq<EmptyTableRef>();
                for (auto &child2: child->nodes) {
                    if (child2->name == "SelectClause") {
                        vector<unique_ptr<ParsedExpression>> select_list;
                        TransformSelectList(child2, select_list);
                        select_node->select_list = std::move(select_list);
                    } else if (child2->name == "FromClause") {
//                        select_node->from_table = make_uniq<EmptyTableRef>();
                        select_node->from_table = TransformFrom(child2);
                    } else if (child2->name == "WhereClause") {
                        select_node->where_clause = TransformWhere(child2);
                    } else if (child2->name == "GroupByClause") {
                        select_node->groups = TransformGroupBy(child2);
                    }
                }
                result->node = std::move(select_node);
                return result;
            }
        }
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }


} // namespace duckdb
