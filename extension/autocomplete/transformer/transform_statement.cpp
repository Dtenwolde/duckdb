#include "duckdb/common/printer.hpp"
#include "duckdb/parser/sql_statement.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

reference<SQLStatement> PEGTransformer::TransformStatement(PEGTransformer &self) {
	throw NotImplementedException("TransformStatement");
	// auto &choice = self.parse_results<ChoiceParseResult>();
	// switch (choice.selected_idx) {
	// case 0:
	// 	return self.TransformUseStatement(self);
	// default:
	// 	throw NotImplementedException("TransformStatement has not been implemented yet.");
	// }
}
} // namespace duckdb
