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

private:
    optional_ptr<PEGTransformer> parent;
    //! Parser options
    ParserOptions &options;
    //! Current stack depth
    idx_t stack_depth;

};

} // namespace duckdb

