#pragma once

#include "duckdb/parser/parser_options.hpp"
namespace duckdb {


class PEGTransformer {

    explicit PEGTransformer(ParserOptions &options);
public:
    static void Transform(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<SQLStatement>> &statements);
    static unique_ptr<SQLStatement> TransformSingleStatement(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<TransactionStatement> TransformTransaction(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<SelectStatement> TransformSelect(std::shared_ptr<peg::Ast> &ast);
    static string TransformIdentifier(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformAliasedExpression(std::vector<std::shared_ptr<peg::Ast>> &aliased_expr_nodes);
    static unique_ptr<ParsedExpression> TransformSingleExpression(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformExpression(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformLiteralExpression(std::shared_ptr<peg::Ast> &ast);
    static void TransformSelectList(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<ParsedExpression>> &select_list);
    static unique_ptr<TableRef> TransformFrom(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformWhere(std::shared_ptr<peg::Ast> &ast);
    static GroupByNode TransformGroupBy(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<CreateStatement> TransformCreateTable(std::shared_ptr<peg::Ast> &ast);
    static void TransformColumnDefinition(std::shared_ptr<peg::Ast> &ast, ColumnList &column_list);
    static unique_ptr<ParsedExpression> TransformColumnReference(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<TableRef> TransformTableReference(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<SQLStatement> TransformInsertStatement(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformFunctionExpression(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformCountStarExpression(std::shared_ptr<peg::Ast> &ast);
    static ExpressionType TransformOperatorToExpressionType(std::shared_ptr<peg::Ast> &operator_node);
    static unique_ptr<ParsedExpression> TransformCaseExpression(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<SQLStatement> TransformPragmaStatement(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<SQLStatement> TransformDescribeStatement(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<QueryNode> TransformSelectNode(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformIsNullExpression(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformCastExpression(std::shared_ptr<peg::Ast> &ast);
    static LogicalType TransformTypeIdentifier(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<OrderModifier> TransformOrderByClause(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<ParsedExpression> TransformSubqueryExpression(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<LimitModifier> TransformLimitClause(std::shared_ptr<peg::Ast> &ast);

    // Helper functions
    static string trimAndRemoveQuotes(std::string_view input);

private:
    optional_ptr<PEGTransformer> parent;
    //! Parser options
    ParserOptions &options;
    //! Current stack depth
    idx_t stack_depth;

};

} // namespace duckdb

