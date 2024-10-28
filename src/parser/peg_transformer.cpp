#include <duckdb/parser/peg_transformer.hpp>
#include <duckdb/parser/statement/transaction_statement.hpp>
#include <duckdb/parser/expression/columnref_expression.hpp>
#include <duckdb/parser/statement/create_statement.hpp>
#include <duckdb/parser/parsed_data/create_table_info.hpp>
#include <duckdb/parser/parsed_data/pragma_info.hpp>
#include <duckdb/parser/expression/function_expression.hpp>
#include <duckdb/parser/tableref/basetableref.hpp>
#include <duckdb/parser/statement/insert_statement.hpp>
#include <duckdb/parser/statement/pragma_statement.hpp>

namespace duckdb {
    PEGTransformer::PEGTransformer(ParserOptions &options)
        : parent(nullptr), options(options), stack_depth(DConstants::INVALID_INDEX) {
    }

    void PEGTransformer::Transform(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<SQLStatement>> &statements) {
        for (auto &statement : ast->nodes) {
            if (statement->name == "SingleStatement") {
                for (auto &child: statement->nodes) {
                    auto result = TransformSingleStatement(child);
                    result->stmt_location = child->position;
                    result->stmt_length = child->length;
                    statements.push_back(std::move(result));
                }
            } else {
                throw NotImplementedException("Transform for " + statement->name + " not implemented");
            }
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
            }
            if (ast->nodes[2]->name == "SelectStatement") {
                result->select_statement = TransformSelect(ast->nodes[2]);
            }
        }
        return result;
    }

    unique_ptr<SQLStatement> PEGTransformer::TransformPragmaStatement(std::shared_ptr<peg::Ast> &ast) {
        auto result = make_uniq<PragmaStatement>();
        auto pragma_info = make_uniq<PragmaInfo>();
        pragma_info->name = TransformIdentifier(ast->nodes[0]);
        for (idx_t i = 1; i < ast->nodes.size(); i++) {
            pragma_info->parameters.push_back(TransformExpression(ast->nodes[i]));
        }
        result->info = std::move(pragma_info);
        return result;
    }

    unique_ptr<SQLStatement> PEGTransformer::TransformDescribeStatement(std::shared_ptr<peg::Ast> &ast) {
        auto result = make_uniq<SelectStatement>();
        auto select_node = make_uniq<SelectNode>();
        select_node->select_list.push_back(make_uniq<StarExpression>());
        auto showref = make_uniq<ShowRef>();
        showref->show_type = ShowType::DESCRIBE;
        if (ast->nodes[0]->name == "Identifier") {
            // TODO implement SUMMARIZE
            showref->table_name = TransformIdentifier(ast->nodes[0]);
        } else if (ast->nodes[0]->name == "SimpleSelect") {
            showref->query = TransformSelectNode(ast->nodes[0]);
        } else {
            throw ParserException("Unexpected node type: " + ast->nodes[0]->name);
        }

        select_node->from_table = std::move(showref);
        result->node = std::move(select_node);

        return result;
    }

    unique_ptr<SQLStatement> PEGTransformer::TransformSingleStatement(std::shared_ptr<peg::Ast> &ast) {
        if (ast->name == "TransactionStatement") {
            return TransformTransaction(ast);
        }
        if (ast->name == "SelectStatement") {
            return TransformSelect(ast);
        }
        if (ast->name == "CreateStatement") {
            // TODO should be TransformCreateStatement
            return TransformCreateTable(ast);
        }
        if (ast->name == "InsertStatement") {
            return TransformInsertStatement(ast);
        }
        if (ast->name == "DeleteStatement") {
            throw NotImplementedException("DeleteStatement not implemented yet.");
        }
        if (ast->name == "UpdateStatement") {
            throw NotImplementedException("UpdateStatement not implemented yet.");
        }
        if (ast->name == "PragmaStatement") {
            return TransformPragmaStatement(ast);
        }
        if (ast->name == "DescribeStatement") {
            return TransformDescribeStatement(ast);
        }
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }

    TransactionType TransformTransactionType(const std::string_view kind) {
        if (kind == "BEGIN TRANSACTION") {
            return TransactionType::BEGIN_TRANSACTION;
        }
        if (kind == "COMMIT") {
            return TransactionType::COMMIT;
        }
        if (kind == "ROLLBACK") {
            return TransactionType::ROLLBACK;
        }
        throw NotImplementedException("Transaction type " + string(kind) + " not implemented yet.");
    }

    unique_ptr<TransactionStatement> PEGTransformer::TransformTransaction(std::shared_ptr<peg::Ast> &ast) {
        auto type = TransformTransactionType(ast->nodes[0]->token);
        auto info = make_uniq<TransactionInfo>(type);
        return make_uniq<TransactionStatement>(std::move(info));
    }

    LogicalType PEGTransformer::TransformTypeIdentifier(std::shared_ptr<peg::Ast> &ast) {
        auto type = StringUtil::Upper(TransformIdentifier(ast->nodes[0]));
        if (type == "REAL") {
            type = "FLOAT";
        }
        return EnumUtil::FromString<LogicalTypeId>(type);
    }

    void PEGTransformer::TransformColumnDefinition(std::shared_ptr<peg::Ast> &ast, ColumnList &column_list) {
        if (ast->nodes.size() != 2) {
            throw ParserException("ColumnDefinition node should have 2 children");
        }
        auto column_name = TransformIdentifier(ast->nodes[0]);
        if (ast->nodes[1]->name == "TypeSpecifier") {
            auto column_type = TransformTypeIdentifier(ast->nodes[1]);
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

    string PEGTransformer::trimAndRemoveQuotes(std::string_view input) {
        // Trim leading and trailing whitespace
        auto start = input.find_first_not_of(' ');
        auto end = input.find_last_not_of(' ');
        if (start == std::string_view::npos || end == std::string_view::npos) {
            return ""; // The input is all spaces
        }
        input = input.substr(start, end - start + 1);

        // Check if the string starts and ends with a single quote
        if (input.size() >= 2 && input.front() == '\'' && input.back() == '\'') {
            // Remove the surrounding single quotes
            input = input.substr(1, input.size() - 2);
        }

        // Convert the result to a std::string
        return string(input);
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformLiteralExpression(std::shared_ptr<peg::Ast> &ast) {
        auto expr_child = ast->nodes[0];
        if (expr_child->name == "StringLiteral") {
            auto string_expr = trimAndRemoveQuotes(expr_child->token);
            return make_uniq<ConstantExpression>(Value(string_expr));
        }
        if (expr_child->name == "NumberLiteral") {
            return make_uniq<ConstantExpression>(Value(stoi(string(expr_child->token))));
        }
        throw NotImplementedException("Transform for " + expr_child->name + " not implemented");
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

    unique_ptr<ParsedExpression> PEGTransformer::TransformCaseExpression(std::shared_ptr<peg::Ast> &ast) {
        // Create a CaseExpression to hold the transformed result
        auto result = make_uniq<CaseExpression>();
        unique_ptr<ParsedExpression> possible_expression;
        bool start_expression = false;

        // Check if the first node is an "Expression" (special case)
        if (!ast->nodes.empty() && ast->nodes[0]->name == "Expression") {
            start_expression = true;
            possible_expression = TransformExpression(ast->nodes[0]);
        }

        // Determine where to start processing "WhenThen" nodes
        size_t case_whenthen_index = start_expression ? 1 : 0;

        // Iterate over the nodes starting from the determined index
        for (idx_t i = case_whenthen_index; i < ast->nodes.size(); ++i) {
            auto &current_node = ast->nodes[i];

            // Handle the last "else" expression
            if (i == ast->nodes.size() - 1 && current_node->name == "Expression") {
                result->else_expr = TransformExpression(current_node);
                continue;
            }

            // Process "WhenThen" nodes
            if (current_node->name == "WhenThen") {
                auto &whenthen = current_node;
                auto when_expression = TransformExpression(whenthen->nodes[0]);
                auto then_expression = TransformExpression(whenthen->nodes[1]);

                // Add a comparison with the possible expression, if present
                if (possible_expression) {
                    when_expression = make_uniq<OperatorExpression>(
                        ExpressionType::COMPARE_EQUAL, possible_expression->Copy(), std::move(when_expression));
                }

                // Add the CaseCheck to the list using an initializer list
                result->case_checks.push_back(CaseCheck{std::move(when_expression), std::move(then_expression)});
            } else {
                throw ParserException("Unexpected type encountered in CASE expression: " + string(current_node->name));
            }
        }

        return result;
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformIsNullExpression(std::shared_ptr<peg::Ast> &ast) {
        unique_ptr<ParsedExpression> result;
        auto col_ref = TransformColumnReference(ast->nodes[0]);
        if (ast->nodes.size() == 2) {
            if (ast->nodes[1]->name == "NotModifier") {
                return make_uniq<OperatorExpression>(ExpressionType::OPERATOR_IS_NOT_NULL, std::move(col_ref));
            }
            throw ParserException("Unexpected type: " + string(ast->nodes[1]->name));
        }
        return make_uniq<OperatorExpression>(ExpressionType::OPERATOR_IS_NULL, std::move(col_ref));
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformCastExpression(std::shared_ptr<peg::Ast> &ast) {
        auto expr_child = TransformExpression(ast->nodes[0]);
        auto cast_type = TransformTypeIdentifier(ast->nodes[1]);
        auto result = make_uniq<CastExpression>(cast_type, std::move(expr_child));
        return result;
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformSubqueryExpression(std::shared_ptr<peg::Ast> &ast) {
        auto result = make_uniq<SubqueryExpression>();
        bool not_modifier = ast->nodes[0]->name == "NotModifier";
        idx_t subquery_index = not_modifier ? 1 : 0;
        result->subquery = TransformSelect(ast->nodes[subquery_index]->nodes[0]);
        result->subquery_type = not_modifier ? SubqueryType::NOT_EXISTS : SubqueryType::EXISTS;
        return result;
    }


    unique_ptr<ParsedExpression> PEGTransformer::TransformSingleExpression(std::shared_ptr<peg::Ast> &ast) {
        auto expr_child = ast->nodes[0];
        if (expr_child->name == "SubqueryExpression") {
            return TransformSubqueryExpression(expr_child);
        }
        if (expr_child->name == "LiteralListExpression") {
            // TODO Figure out how to deal with list of expressions
            return TransformExpression(expr_child->nodes[0]);
        }
        if (expr_child->name == "ParenthesisExpression") {
            throw NotImplementedException("ParenthesisExpression not implemented yet.");
        }
        if (expr_child->name == "DateExpression") {
            throw NotImplementedException("DateExpression not implemented yet.");
        }
        if (expr_child->name == "DistinctExpression") {
            throw NotImplementedException("DistinctExpression not implemented yet.");
        }
        if (expr_child->name == "SubstringExpression") {
            throw NotImplementedException("SubstringExpression not implemented yet.");
        }
        if (expr_child->name == "IsNullExpression") {
            return TransformIsNullExpression(expr_child);
        }
        if (expr_child->name == "CaseExpression") {
            return TransformCaseExpression(expr_child);
        }
        if (expr_child->name == "CountStarExpression") {
            return TransformCountStarExpression(expr_child);
        }
        if (expr_child->name == "CastExpression") {
            return TransformCastExpression(expr_child);
        }
        if (expr_child->name == "ExtractExpression") {
            throw NotImplementedException("ExtractExpression not implemented yet.");
        }
        if (expr_child->name == "WindowExpression") {
            throw NotImplementedException("WindowExpression not implemented yet.");
        }
        if (expr_child->name == "FunctionExpression") {
            return TransformFunctionExpression(expr_child);
        }
        if (expr_child->name == "ColumnReference") {
            return TransformColumnReference(expr_child);
        } if (expr_child->name == "LiteralExpression") {
            return TransformLiteralExpression(expr_child);
        } if (expr_child->name == "NullExpression") {
            return make_uniq<ConstantExpression>(Value("NULL"));
        } if (expr_child->name == "TrueFalseExpression") {
            if (expr_child->token == "true") {
                return make_uniq<ConstantExpression>(Value("true"));
            }
            return make_uniq<ConstantExpression>(Value("false"));
        }
        throw NotImplementedException("Transform for " + expr_child->name + " not implemented");
    }

    ExpressionType PEGTransformer::TransformOperatorToExpressionType(std::shared_ptr<peg::Ast> &operator_node) {
        // Extract the operator string (e.g., "+", ">", "AND")
        auto operator_str = string(operator_node->nodes[0]->token);

        // Comparison operators
        if (operator_str == "=") {
            return ExpressionType::COMPARE_EQUAL;
        }
        if (operator_str == "<=") {
            return ExpressionType::COMPARE_LESSTHANOREQUALTO;
        }
        if (operator_str == ">=") {
            return ExpressionType::COMPARE_GREATERTHANOREQUALTO;
        }
        if (operator_str == "<") {
            return ExpressionType::COMPARE_LESSTHAN;
        }
        if (operator_str == ">") {
            return ExpressionType::COMPARE_GREATERTHAN;
        }

        // Boolean operators
        if (operator_str == "AND") {
            return ExpressionType::CONJUNCTION_AND;
        }
        if (operator_str == "OR") {
            return ExpressionType::CONJUNCTION_OR;
        }

        // Like operator
        if (operator_str == "LIKE") {
            throw NotImplementedException("LIKE operator not implemented yet.");
        }
        if (operator_str == "NOT LIKE") {
            throw NotImplementedException("NOT LIKE operator not implemented yet.");
        }

        // IN operator
        if (operator_str == "IN") {
            return ExpressionType::COMPARE_IN;
        }
        if (operator_str == "NOT IN") {
            return ExpressionType::COMPARE_NOT_IN;
        }

        // Between operator
        if (operator_str == "BETWEEN") {
            return ExpressionType::COMPARE_BETWEEN;
        }
        if (operator_str == "NOT BETWEEN") {
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
        // Initialize the not_modifier flag
        bool not_modifier = false;

        // Check if the first node is a "NotModifier"
        if (!ast->nodes.empty() && ast->nodes[0]->name == "NotModifier") {
            not_modifier = true;
        }

        // Check if the last node is a "SingleExpression"
        size_t single_expr_index = not_modifier ? 1 : 0;
        if (ast->nodes.size() > single_expr_index && ast->nodes[single_expr_index]->name == "SingleExpression") {
            left_expr = TransformSingleExpression(ast->nodes[single_expr_index]);
            if (not_modifier) {
                left_expr = make_uniq<OperatorExpression>(ExpressionType::OPERATOR_NOT, std::move(left_expr));
            }
        } else {
            throw NotImplementedException("Expression must start with a SingleExpression");
        }

        // Now, iterate through the rest to find (Operator, SingleExpression) pairs
        for (size_t i = single_expr_index + 1; i < ast->nodes.size(); i += 2) {
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
                vector<unique_ptr<ParsedExpression> > children;
                children.push_back(std::move(left_expr));
                children.push_back(std::move(right_expr));
                result = make_uniq<FunctionExpression>(string(operator_type->token), std::move(children));
            } else if (operator_type->name == "ComparisonOperator") {
                result = make_uniq<ComparisonExpression>(OperatorToExpressionType(string(operator_type->token)), std::move(left_expr), std::move(right_expr));
            } else if (operator_type->name == "BooleanOperator") {
                if (StringUtil::Upper(string(operator_type->token)) == "AND") {
                    result = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_AND, std::move(left_expr), std::move(right_expr));
                } else if (StringUtil::Upper(string(operator_type->token)) == "OR") {
                    result = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_OR, std::move(left_expr), std::move(right_expr));
                } else {
                    throw NotImplementedException("Operator type " + string(operator_type->token) + " not implemented yet.");
                }
            } else if (operator_type->name == "LikeOperator") {
                bool not_like_modifier = false;
                if (operator_type->nodes[0]->name == "NotModifier") {
                    not_like_modifier = true;
                }
                string function_name = not_like_modifier ? "!" : "";
                idx_t expr_idx = not_like_modifier ? 1 : 0;
                vector<unique_ptr<ParsedExpression> > children;
                children.push_back(std::move(left_expr));
                children.push_back(std::move(right_expr));
                if (StringUtil::Upper(string(operator_type->nodes[expr_idx]->token)) == "ILIKE") {
                    function_name += "~~*";
                } else if (StringUtil::Upper(string(operator_type->nodes[expr_idx]->token)) == "LIKE") {
                    function_name += not_like_modifier ? "~~" : "~~~";
                }
                result = make_uniq<FunctionExpression>(function_name, std::move(children));
            } else if (operator_type->name == "InOperator") {
                throw NotImplementedException("InOperator not implemented yet.");
            } else if (operator_type->name == "BetweenOperator") {
                throw NotImplementedException("BetweenOperator not implemented yet.");
            } else if (operator_type->name == "WindowOperator") {
                throw NotImplementedException("WindowOperator not implemented yet.");
            } else {
                throw NotImplementedException("Operator not implemented yet.");
            }
            // Update the left_expr to the result so it can be used for the next iteration
            left_expr = std::move(result);
        }


        return left_expr;
    }

    string PEGTransformer::TransformIdentifier(std::shared_ptr<peg::Ast> &ast) {
        return string(ast->nodes[0]->token);
    }

    unique_ptr<ParsedExpression>
    PEGTransformer::TransformAliasedExpression(std::vector<std::shared_ptr<peg::Ast> > &aliased_expr_nodes) {
        unique_ptr<ParsedExpression> expr;
        string alias;

        if (aliased_expr_nodes[0]->name == "Expression") {
            expr = TransformExpression(aliased_expr_nodes[0]);
        } else {
            throw NotImplementedException("Not implemented yet for Aliased Expression.");
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
                                             vector<unique_ptr<ParsedExpression> > &select_list) {
        for (auto &child: ast->nodes) {
            if (child->name == "AliasedExpression") {
                select_list.push_back(TransformAliasedExpression(child->nodes));
            } else if (child->name == "StarToken") {
                select_list.push_back(make_uniq<StarExpression>());
            }
        }
    }

    unique_ptr<TableRef> PEGTransformer::TransformTableReference(std::shared_ptr<peg::Ast> &ast) {
        unique_ptr<TableRef> result;
        if (ast->nodes[0]->name == "Identifier") {
            auto base_table_ref = make_uniq<BaseTableRef>();
            base_table_ref->table_name = TransformIdentifier(ast->nodes[0]);
            if (ast->nodes.size() == 1) {
                return base_table_ref;
            }
            result = std::move(base_table_ref);
        } else if (ast->nodes[0]->name == "FunctionExpression") {
            auto table_function_ref = make_uniq<TableFunctionRef>();
            auto function_name = TransformIdentifier(ast->nodes[0]->nodes[0]);
            vector<unique_ptr<ParsedExpression>> function_children;
            for (idx_t i = 1; i < ast->nodes[0]->nodes.size(); i++) {
                function_children.push_back(TransformExpression(ast->nodes[0]->nodes[i]));
            }
            // TODO handle subquery as a tablefunctionref
            table_function_ref->function = make_uniq<FunctionExpression>(function_name, std::move(function_children));
            result = std::move(table_function_ref);
        } else {
            throw NotImplementedException("Transform for " + ast->nodes[0]->name + " not implemented");
        }
        if (ast->nodes.size() == 1) {
            return result;
        }
        if (ast->nodes[1]->name == "Identifier") {
            result->alias = TransformIdentifier(ast->nodes[1]);
        } else {
            throw ParserException("Expected Identifier");
        }
        return result;
    }

    unique_ptr<TableRef> PEGTransformer::TransformFrom(std::shared_ptr<peg::Ast> &ast) {
        unique_ptr<TableRef> result =  TransformTableReference(ast->nodes[0]);
        if (ast->nodes.size() == 1) {
            return result;
        }

        for (idx_t i = 1; i < ast->nodes.size(); i++) {
            if (ast->nodes[i]->name == "TableReference") {
                auto joinref = make_uniq<JoinRef>(JoinRefType::CROSS);
                joinref->left = std::move(result);
                joinref->right = TransformTableReference(ast->nodes[i]);
                result = std::move(joinref);
            } else if (ast->nodes[i]->name == "ExplicitJoin") {
                throw NotImplementedException("Explicit Join not implemented yet.");
            }
        }
        return result;
    }

    unique_ptr<ParsedExpression> PEGTransformer::TransformWhere(std::shared_ptr<peg::Ast> &ast) {
        return TransformExpression(ast->nodes[0]);
    }

    GroupByNode PEGTransformer::TransformGroupBy(std::shared_ptr<peg::Ast> &ast) {
        GroupByNode result;
        for (auto &child: ast->nodes) {
            result.group_expressions.push_back(TransformExpression(child));
        }
        return result;
    }

    unique_ptr<OrderModifier> PEGTransformer::TransformOrderByClause(std::shared_ptr<peg::Ast> &ast) {
        vector<OrderByNode> order_by_nodes;
        for (auto &orderby_expr: ast->nodes) {
            // TODO expand to support sorting order and nulls handling
            auto expr = TransformExpression(orderby_expr->nodes[0]);
            order_by_nodes.push_back(OrderByNode(OrderType::ORDER_DEFAULT,
                OrderByNullType::ORDER_DEFAULT, std::move(expr)));
        }

        auto result = make_uniq<OrderModifier>();
        result->orders = std::move(order_by_nodes);
        return result;
    }

    unique_ptr<LimitModifier> PEGTransformer::TransformLimitClause(std::shared_ptr<peg::Ast> &ast) {
        auto result = make_uniq<LimitModifier>();
        result->limit = TransformExpression(ast->nodes[0]);
        // TODO support OFFSET
        return result;
    }

    unique_ptr<QueryNode> PEGTransformer::TransformSelectNode(std::shared_ptr<peg::Ast> &ast) {
        auto select_node = make_uniq<SelectNode>();
        select_node->from_table = make_uniq<EmptyTableRef>();
        for (auto &child2: ast->nodes) {
            if (child2->name == "SelectClause") {
                vector<unique_ptr<ParsedExpression> > select_list;
                TransformSelectList(child2, select_list);
                select_node->select_list = std::move(select_list);
            } else if (child2->name == "FromClause") {
                select_node->from_table = TransformFrom(child2);
            } else if (child2->name == "WhereClause") {
                select_node->where_clause = TransformWhere(child2);
            } else if (child2->name == "GroupByClause") {
                select_node->groups = TransformGroupBy(child2);
            } else if (child2->name == "OrderByClause") {
                select_node->modifiers.push_back(TransformOrderByClause(child2));
            } else if (child2->name == "LimitClause") {
                select_node->modifiers.push_back(TransformLimitClause(child2));
            }
        }

        return select_node;
    }

    unique_ptr<SelectStatement> PEGTransformer::TransformSelect(std::shared_ptr<peg::Ast> &ast) {
        for (auto &child: ast->nodes) {
            if (child->name == "SimpleSelect") {
                auto result = make_uniq<SelectStatement>();
                result->node = TransformSelectNode(child);
                return result;
            }
        }
        throw NotImplementedException("Transform for " + ast->name + " not implemented");
    }
} // namespace duckdb
