#pragma once

#include "duckdb/parser/parser_options.hpp"
namespace duckdb {


class PEGTransformer {

    explicit PEGTransformer(ParserOptions &options);
public:
    static void Transform(std::shared_ptr<peg::Ast> &ast, vector<unique_ptr<SQLStatement>> &statements);
    static unique_ptr<SQLStatement> TransformSingleStatement(std::shared_ptr<peg::Ast> &ast);
    static unique_ptr<TransactionStatement> TransformTransaction(std::shared_ptr<peg::Ast> &ast);

private:
    optional_ptr<PEGTransformer> parent;
    //! Parser options
    ParserOptions &options;
    //! Current stack depth
    idx_t stack_depth;

};

} // namespace duckdb

