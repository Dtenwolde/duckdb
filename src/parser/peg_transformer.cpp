#include <duckdb/parser/peg_transformer.hpp>
#include <duckdb/parser/statement/transaction_statement.hpp>
#include <duckdb/parser/expression/columnref_expression.hpp>
#include <duckdb/parser/statement/create_statement.hpp>
#include <duckdb/parser/parsed_data/create_table_info.hpp>
#include <duckdb/common/enums/catalog_type.hpp>
#include <duckdb/parser/tableref/basetableref.hpp>
#include <duckdb/parser/statement/insert_statement.hpp>

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

    unique_ptr<SQLStatement> PEGTransformer::TransformInsertStatement(std::shared_ptr<peg::Ast> &ast) {
        auto result = make_uniq<InsertStatement>();
        if (ast->nodes[0]->name == "Identifier") {
            result->table = TransformIdentifier(ast->nodes[0]);
        } else {
            throw ParserException("InsertStatement node should have Identifier as first child");
        }
        if (ast->nodes.size() == 2) {
            throw NotImplementedException("InsertStatement with 2 children not implemented yet.");
        } else if (ast->nodes.size() == 3) {
            if (ast->nodes[1]->name != "ColumnList") {
                throw ParserException("Column list is missing in InsertStatement");
            }
            for (auto &child: ast->nodes[1]->nodes) {
                if (child->name == "Identifier") {
                    result->columns.push_back(TransformIdentifier(child));
                } else {
                    throw ParserException("ColumnList node should have Identifier as children");
                }
            }
            if (ast->nodes[2]->name == "ValuesList") {
                auto expression_list = make_uniq<ExpressionListRef>();
                auto values_list = ast->nodes[2];
                for (auto &child: values_list->nodes) {
                    if (child->name != "Value") {
                        throw ParserException("ValuesList node should have Value as children");
                    }
                    vector<unique_ptr<ParsedExpression>> values;
                    for (auto &child2: child->nodes) {
                        if (child2->name == "Expression") {
                            values.push_back(TransformExpression(child2));
                        }
                    }
                    expression_list->values.push_back(std::move(values));
                }
                auto select_statement = make_uniq<SelectStatement>();
                auto select_node = make_uniq<SelectNode>();
                select_node->from_table = std::move(expression_list);
                select_node->select_list.push_back(make_uniq<StarExpression>());
                select_statement->node = std::move(select_node);
                result->select_statement = std::move(select_statement);

                return result;
            } else if (ast->nodes[2]->name == "SelectStatement") {
                result->select_statement = TransformSelect(ast->nodes[2]);
            }
        }
        return result;
    }

    unique_ptr<SQLStatement> PEGTransformer::TransformSingleStatement(std::shared_ptr<peg::Ast> &ast) {
        if (ast->name == "TransactionStatement") {
            return TransformTransaction(ast);
        } else if (ast->name == "SelectStatement") {
            return TransformSelect(ast);
        } else if (ast->name == "CreateStatement") {
            // TODO should be TransformCreateStatement
            return TransformCreateTable(ast);
        } else if (ast->name == "InsertStatement") {
            return TransformInsertStatement(ast);
        } else if (ast->name == "DeleteStatement") {
            throw NotImplementedException("DeleteStatement not implemented yet.");
        } else if (ast->name == "UpdateStatement") {
            throw NotImplementedException("UpdateStatement not implemented yet.");
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
        auto name = TransformIdentifier(ast->nodes[0]);

        // Two elements means separated by a dot, therefore table_name is first, col_name is second
        if (ast->nodes.size() == 2) {
            auto col_name = TransformIdentifier(ast->nodes[1]);
            auto result = make_uniq<ColumnRefExpression>(col_name, name);
            return result;
        }
        return make_uniq<ColumnRefExpression>(name);
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformFunctionExpression(std::shared_ptr<peg::Ast> &ast) {
        auto function_name = TransformIdentifier(ast->nodes[0]);
        vector<unique_ptr<ParsedExpression>> function_children;
        for (idx_t i = 1; i < ast->nodes.size(); i++) {
            if (ast->nodes[i]->name == "Expression") {
                function_children.push_back(TransformExpression(ast->nodes[i]));
            } else {
                throw ParserException("FunctionExpression node should have Expression as children");
            }
        }
        return make_uniq<FunctionExpression>(function_name, std::move(function_children));
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformCountStarExpression(std::shared_ptr<peg::Ast> &ast) {
        return make_uniq<FunctionExpression>("COUNT", vector<unique_ptr<ParsedExpression>>());
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformSingleExpression(std::shared_ptr<peg::Ast> &ast) {
        auto expr_child = ast->nodes[0];
        if (expr_child->name == "SubqueryExpression") {
            throw NotImplementedException("SubqueryExpression not implemented yet.");
        } else if (expr_child->name == "LiteralListExpression") {
            // TODO Figure out how to deal with list of expressions
            return TransformExpression(expr_child->nodes[0]);
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
            return TransformCountStarExpression(expr_child);
        } else if (expr_child->name == "CastExpression") {
            throw NotImplementedException("CastExpression not implemented yet.");
        } else if (expr_child->name == "ExtractExpression") {
            throw NotImplementedException("ExtractExpression not implemented yet.");
        } else if (expr_child->name == "WindowExpression") {
            throw NotImplementedException("WindowExpression not implemented yet.");
        } else if (expr_child->name == "FunctionExpression") {
            return TransformFunctionExpression(expr_child);
        } else if (expr_child->name == "ColumnReference") {
            return TransformColumnReference(expr_child);
        } else if (expr_child->name == "LiteralExpression") {
            return TransformLiteralExpression(expr_child);
        } else {
            throw NotImplementedException("Transform for " + ast->name + " not implemented");
        }
    }

    ExpressionType PEGTransformer::TransformOperatorToExpressionType(std::shared_ptr<peg::Ast> &operator_node) {
        // Extract the operator string (e.g., "+", ">", "AND")
        auto operator_str = string(operator_node->nodes[0]->token);

        // Comparison operators
        if (operator_str == "=") {
            return ExpressionType::COMPARE_EQUAL;
        } else if (operator_str == "<=") {
            return ExpressionType::COMPARE_LESSTHANOREQUALTO;
        } else if (operator_str == ">=") {
            return ExpressionType::COMPARE_GREATERTHANOREQUALTO;
        } else if (operator_str == "<") {
            return ExpressionType::COMPARE_LESSTHAN;
        } else if (operator_str == ">") {
            return ExpressionType::COMPARE_GREATERTHAN;
        }

        // Boolean operators
        if (operator_str == "AND") {
            return ExpressionType::CONJUNCTION_AND;
        } else if (operator_str == "OR") {
            return ExpressionType::CONJUNCTION_OR;
        }

        // Like operator
        if (operator_str == "LIKE") {
            throw NotImplementedException("LIKE operator not implemented yet.");
        } else if (operator_str == "NOT LIKE") {
            throw NotImplementedException("NOT LIKE operator not implemented yet.");
        }

        // IN operator
        if (operator_str == "IN") {
            return ExpressionType::COMPARE_IN;
        } else if (operator_str == "NOT IN") {
            return ExpressionType::COMPARE_NOT_IN;
        }

        // Between operator
        if (operator_str == "BETWEEN") {
            return ExpressionType::COMPARE_BETWEEN;
        } else if (operator_str == "NOT BETWEEN") {
            return ExpressionType::COMPARE_NOT_BETWEEN;
        }

        // Window operator
        if (operator_str == "OVER") {
            return ExpressionType::WINDOW_AGGREGATE;
        }

        throw NotImplementedException("Unsupported operator: " + operator_str);
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformExpression(std::shared_ptr<peg::Ast> &ast) {
        unique_ptr<ParsedExpression> result;
        unique_ptr<ParsedExpression> left_expr;

        // Transform the first SingleExpression
        if (!ast->nodes.empty() && ast->nodes[0]->name == "SingleExpression") {
            left_expr = TransformSingleExpression(ast->nodes[0]);
        } else {
            throw NotImplementedException("Expression must start with a SingleExpression");
        }

        // Now, iterate through the rest to find (Operator, SingleExpression) pairs
        for (size_t i = 1; i < ast->nodes.size(); i += 2) {
            auto &operator_node = ast->nodes[i];
            auto &right_expr_node = ast->nodes[i + 1];

            if (operator_node->name != "Operator" || right_expr_node->name != "SingleExpression") {
                throw NotImplementedException("Expected Operator followed by SingleExpression");
            }

            // Transform the right-hand SingleExpression
            auto right_expr = TransformSingleExpression(right_expr_node);

            // Transform the operator into an ExpressionType
            auto operator_type = operator_node->nodes[0];
            if (operator_type->name == "ArithmeticOperator") {
                vector<unique_ptr<ParsedExpression>> children;
                children.push_back(std::move(left_expr));
                children.push_back(std::move(right_expr));
                result = make_uniq<FunctionExpression>(string(operator_type->token), std::move(children));
            } else if (operator_type->name == "ComparisonOperator") {
                throw NotImplementedException("ComparisonOperator not implemented yet.");
            } else if (operator_type->name == "BooleanOperator") {
                throw NotImplementedException("BooleanOperator not implemented yet.");
            } else if (operator_type->name == "LikeOperator") {
                throw NotImplementedException("LikeOperator not implemented yet.");
            } else if (operator_type->name == "InOperator") {
                throw NotImplementedException("InOperator not implemented yet.");
            } else if (operator_type->name == "BetweenOperator") {
                throw NotImplementedException("BetweenOperator not implemented yet.");
            } else if (operator_type->name == "WindowOperator") {
                throw NotImplementedException("WindowOperator not implemented yet.");
            } else {
                throw NotImplementedException("Operator not implemented yet.");
            }
//            auto operator_type = TransformOperatorToExpressionType(operator_node);



//            // Use ComparisonExpression for comparison operators
//            if (operator_type == ExpressionType::COMPARE_EQUAL ||
//                operator_type == ExpressionType::COMPARE_LESSTHAN ||
//                operator_type == ExpressionType::COMPARE_GREATERTHAN ||
//                operator_type == ExpressionType::COMPARE_LESSTHANOREQUALTO ||
//                operator_type == ExpressionType::COMPARE_GREATERTHANOREQUALTO ||
//                operator_type == ExpressionType::COMPARE_NOTEQUAL ||
//                operator_type == ExpressionType::COMPARE_IN ||
//                operator_type == ExpressionType::COMPARE_NOT_IN) {
//
//                result = make_uniq<ComparisonExpression>(operator_type, std::move(left_expr), std::move(right_expr));
//            }
                // Use OperatorExpression for non-comparison operators
//            else {
//                result = make_uniq<OperatorExpression>(operator_type, std::move(left_expr), std::move(right_expr));
//            }

            // Update the left_expr to the result so it can be used for the next iteration
            left_expr = std::move(result);
        }

        return left_expr;
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
            if (ast->nodes.size() == 1) {
                return base_table_ref;
            }
            if (ast->nodes[1]->name == "Identifier") {
                base_table_ref->alias = TransformIdentifier(ast->nodes[1]);
                return base_table_ref;
            }
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
        return TransformExpression(ast->nodes[0]);
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