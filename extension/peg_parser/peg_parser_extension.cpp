#include "../autocomplete/include/peg_parser_override.hpp"
#include "duckdb.hpp"
#include "include/peg_parser_extension.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());

	// Install the parser override
	if (!config.parser_override) { // Only install if no other override is present
		config.parser_override = make_uniq<PEGParserOverride>();
	}
}

void PegParserExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string PegParserExtension::Name() {
	return "peg_parser";
}
std::string PegParserExtension::Version() const {
	return Extension::Version();
}

} // namespace duckdb

extern "C" {
	DUCKDB_CPP_EXTENSION_ENTRY(duckdb_sql, loader) {
		duckdb::LoadInternal(loader);
	}
}