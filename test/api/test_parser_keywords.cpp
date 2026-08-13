#include "catch.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_callback_manager.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"

using namespace duckdb;

struct TestParserExtensionData : public ParserExtensionParseData {
	unique_ptr<ParserExtensionParseData> Copy() const override {
		return make_uniq<TestParserExtensionData>();
	}

	string ToString() const override {
		return "API_EXTENSION";
	}
};

static unique_ptr<ParserExtensionParseData> TransformTestGrammarExtensionBody(ParserExtensionInfo *, PEGTransformer &,
                                                                              ParseResult &) {
	return make_uniq<TestParserExtensionData>();
}

static unique_ptr<ParserExtensionParseData>
TransformTestGrammarExtension(ParserExtensionInfo *, PEGTransformer &transformer, ParseResult &parse_result) {
	auto &list = parse_result.Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<ParserExtensionParseData>>(list.GetChild(1));
}

TEST_CASE("Parser keyword registration validates keyword spelling", "[api][parser]") {
	ExtensionCallbackManager manager;

	for (const auto &keyword : {ExtensionKeyword {"ab", ExtensionKeywordCategory::RESERVED},
	                            ExtensionKeyword {"__", ExtensionKeywordCategory::RESERVED},
	                            ExtensionKeyword {"valid$keyword", ExtensionKeywordCategory::UNRESERVED}}) {
		REQUIRE_NOTHROW(manager.Register(vector<ExtensionKeyword> {keyword}));
	}

	for (const auto &keyword : {"", "a", "_", "1", "12", "12.5", "1keyword", "key1word", "keyword1", "$keyword",
	                            "key-word", "two words", "schema.word", "\"quoted\"", "keyword/operator"}) {
		REQUIRE_THROWS_AS(manager.Register(vector<ExtensionKeyword> {{keyword, ExtensionKeywordCategory::RESERVED}}),
		                  InvalidInputException);
	}
}

TEST_CASE("Parser keyword batches are registered atomically", "[api][parser]") {
	ExtensionCallbackManager manager;
	vector<ExtensionKeyword> keywords {{"batch_keyword", ExtensionKeywordCategory::RESERVED},
	                                   {"batch_keyword", ExtensionKeywordCategory::UNRESERVED}};

	REQUIRE_THROWS_AS(manager.Register(keywords), InvalidInputException);
	REQUIRE_FALSE(manager.GetKeywordExtension()->IsKeyword("batch_keyword"));
	REQUIRE_FALSE(manager.HasParserExtensions());
}

TEST_CASE("Promoted built-in parser keywords do not duplicate metadata", "[api][parser]") {
	ExtensionCallbackManager manager;
	manager.Register(vector<ExtensionKeyword> {{"generated", ExtensionKeywordCategory::TYPE_NAME}});

	idx_t generated_count = 0;
	for (const auto &keyword : manager.GetKeywordExtension()->KeywordList()) {
		if (keyword.name == "generated" && keyword.category == KeywordCategory::KEYWORD_TYPE_FUNC) {
			generated_count++;
		}
	}
	REQUIRE(generated_count == 1);
}

TEST_CASE("Grammar extensions can add a statement alternative", "[api][parser]") {
	ExtensionCallbackManager manager;
	ParserExtension extension;
	extension.grammar_extension.grammar = "ApiExtensionStatement <- 'API_EXTENSION' ApiExtensionBody\n"
	                                      "ApiExtensionBody <- Expression\n";
	extension.grammar_extension.statement_rule = "ApiExtensionStatement";
	extension.grammar_extension.RegisterTransformer("ApiExtensionStatement", TransformTestGrammarExtension);
	extension.grammar_extension.RegisterTransformer("ApiExtensionBody", TransformTestGrammarExtensionBody);
	manager.Register(std::move(extension));

	for (const auto trampoline : {false, true}) {
		ParserOptions options;
		options.parser_extensions = manager.GetParserExtensions();
		options.debug_transformer_trampoline_style = trampoline;
		Parser parser(std::move(options));
		REQUIRE_NOTHROW(parser.ParseQuery("API_EXTENSION 42"));
		REQUIRE(parser.statements.size() == 1);
		REQUIRE(parser.statements[0]->type == StatementType::EXTENSION_STATEMENT);
	}
}

TEST_CASE("Grammar extension registration is atomic", "[api][parser]") {
	ExtensionCallbackManager manager;
	{
		GrammarExtension extension;
		REQUIRE_THROWS(extension.RegisterTransformer("NullRule", nullptr));
		extension.RegisterTransformer("DuplicateRule", TransformTestGrammarExtension);
		REQUIRE_THROWS(extension.RegisterTransformer("duplicaterule", TransformTestGrammarExtensionBody));
	}
	for (const auto &entry :
	     vector<pair<string, string>> {{"DefinedRule <- 'DEFINED'\n", "MissingRule"},
	                                   {"NullableRule <- 'OPTIONAL'?\n", "NullableRule"},
	                                   {"ExtensionStatement <- 'INVALID'\n", "ExtensionStatement"}}) {
		ParserExtension extension;
		extension.grammar_extension.grammar = entry.first;
		extension.grammar_extension.statement_rule = entry.second;
		extension.grammar_extension.RegisterTransformer(entry.second, TransformTestGrammarExtension);
		REQUIRE_THROWS(manager.Register(std::move(extension)));
	}
	{
		ParserExtension extension;
		extension.grammar_extension.grammar = "RootRule <- ChildRule\nChildRule <- 'CHILD'\n";
		extension.grammar_extension.statement_rule = "RootRule";
		extension.grammar_extension.RegisterTransformer("ChildRule", TransformTestGrammarExtensionBody);
		REQUIRE_THROWS(manager.Register(std::move(extension)));
	}
	{
		ParserExtension extension;
		extension.grammar_extension.grammar = "RootRule <- 'ROOT'\n";
		extension.grammar_extension.statement_rule = "RootRule";
		extension.grammar_extension.RegisterTransformer("RootRule", TransformTestGrammarExtension);
		extension.grammar_extension.RegisterTransformer("MissingRule", TransformTestGrammarExtensionBody);
		REQUIRE_THROWS(manager.Register(std::move(extension)));
	}
	REQUIRE_FALSE(manager.HasParserExtensions());
}
