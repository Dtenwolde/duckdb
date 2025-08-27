import os
import re

# The C++ code for the factory constructor
cpp_code = """
PEGTransformerFactory::PEGTransformerFactory() {
    // Registering transform functions using the macro for brevity
    REGISTER_TRANSFORM(TransformStatement);
    REGISTER_TRANSFORM(TransformUseStatement);

    REGISTER_TRANSFORM(TransformSetStatement);
    REGISTER_TRANSFORM(TransformResetStatement);
    REGISTER_TRANSFORM(TransformDeleteStatement);
    REGISTER_TRANSFORM(TransformPragmaStatement);
    REGISTER_TRANSFORM(TransformPragmaAssign);
    REGISTER_TRANSFORM(TransformPragmaFunction);
    REGISTER_TRANSFORM(TransformPragmaParameters);
    REGISTER_TRANSFORM(TransformUseTarget);

    REGISTER_TRANSFORM(TransformCopyStatement);
    REGISTER_TRANSFORM(TransformCopyTable);
    REGISTER_TRANSFORM(TransformCopySelect);
    REGISTER_TRANSFORM(TransformFromOrTo);
    REGISTER_TRANSFORM(TransformCopyFileName);
    REGISTER_TRANSFORM(TransformIdentifierColId);
    REGISTER_TRANSFORM(TransformCopyOptions);

    REGISTER_TRANSFORM(TransformDetachStatement);
    REGISTER_TRANSFORM(TransformAttachStatement);
    REGISTER_TRANSFORM(TransformAttachAlias);
    REGISTER_TRANSFORM(TransformAttachOptions);
    REGISTER_TRANSFORM(TransformGenericCopyOptionList);
    REGISTER_TRANSFORM(TransformGenericCopyOption);
    REGISTER_TRANSFORM(TransformGenericCopyOptionListParens);
    REGISTER_TRANSFORM(TransformSpecializedOptionList);
    REGISTER_TRANSFORM(TransformSpecializedOption);
    REGISTER_TRANSFORM(TransformSingleOption);
    REGISTER_TRANSFORM(TransformEncodingOption);
    REGISTER_TRANSFORM(TransformForceQuoteOption);
    REGISTER_TRANSFORM(TransformCopyFromDatabase);
    REGISTER_TRANSFORM(TransformCopyDatabaseFlag);

    REGISTER_TRANSFORM(TransformInsertColumnList);
    REGISTER_TRANSFORM(TransformColumnList);

    REGISTER_TRANSFORM(TransformCreateStatement);
    REGISTER_TRANSFORM(TransformCreateStatementVariation);
    REGISTER_TRANSFORM(TransformCreateTableStmt);
    REGISTER_TRANSFORM(TransformCreateColumnList);
    REGISTER_TRANSFORM(TransformCreateTableColumnList);

    REGISTER_TRANSFORM(TransformColumnDefinition);
    REGISTER_TRANSFORM(TransformTypeOrGenerated);
    REGISTER_TRANSFORM(TransformType);
    REGISTER_TRANSFORM(TransformNumericType);
    REGISTER_TRANSFORM(TransformSimpleNumericType);
    REGISTER_TRANSFORM(TransformDecimalNumericType);
    REGISTER_TRANSFORM(TransformFloatType);
    REGISTER_TRANSFORM(TransformDecimalType);
    Register("DecType", &TransformDecimalType);
    Register("NumericModType", &TransformDecimalType);

    REGISTER_TRANSFORM(TransformSimpleType);
    REGISTER_TRANSFORM(TransformQualifiedTypeName);
    REGISTER_TRANSFORM(TransformCharacterType);

    REGISTER_TRANSFORM(TransformTypeModifiers);

    REGISTER_TRANSFORM(TransformTopLevelConstraint);
    REGISTER_TRANSFORM(TransformTopLevelConstraintList);
    REGISTER_TRANSFORM(TransformTopPrimaryKeyConstraint);
    REGISTER_TRANSFORM(TransformTopUniqueConstraint);
    REGISTER_TRANSFORM(TransformCheckConstraint);
    REGISTER_TRANSFORM(TransformTopForeignKeyConstraint);

    REGISTER_TRANSFORM(TransformColumnIdList);

    REGISTER_TRANSFORM(TransformCheckpointStatement);
    REGISTER_TRANSFORM(TransformExportStatement);
    REGISTER_TRANSFORM(TransformImportStatement);
    REGISTER_TRANSFORM(TransformExportSource);

    REGISTER_TRANSFORM(TransformTransactionStatement);
    REGISTER_TRANSFORM(TransformBeginTransaction);
    REGISTER_TRANSFORM(TransformRollbackTransaction);

    REGISTER_TRANSFORM(TransformCommitTransaction);
    REGISTER_TRANSFORM(TransformReadOrWrite);

    REGISTER_TRANSFORM(TransformLoadStatement);
    REGISTER_TRANSFORM(TransformInstallStatement);
    REGISTER_TRANSFORM(TransformFromSource);
    REGISTER_TRANSFORM(TransformVersionNumber);

    REGISTER_TRANSFORM(TransformDeallocateStatement);

    REGISTER_TRANSFORM(TransformCallStatement);
    REGISTER_TRANSFORM(TransformTableFunctionArguments);
    REGISTER_TRANSFORM(TransformFunctionArgument);
    REGISTER_TRANSFORM(TransformNamedParameter);

    REGISTER_TRANSFORM(TransformTruncateStatement);
    REGISTER_TRANSFORM(TransformBaseTableName);
    REGISTER_TRANSFORM(TransformSchemaReservedTable);
    REGISTER_TRANSFORM(TransformCatalogReservedSchemaTable);
    REGISTER_TRANSFORM(TransformSchemaQualification);
    REGISTER_TRANSFORM(TransformCatalogQualification);
    REGISTER_TRANSFORM(TransformQualifiedName);
    REGISTER_TRANSFORM(TransformSchemaReservedIdentifierOrStringLiteral);
    REGISTER_TRANSFORM(TransformCatalogReservedSchemaIdentifierOrStringLiteral);
    REGISTER_TRANSFORM(TransformTableNameIdentifierOrStringLiteral);

    REGISTER_TRANSFORM(TransformStandardAssignment);
    REGISTER_TRANSFORM(TransformSetAssignment);
    REGISTER_TRANSFORM(TransformSettingOrVariable);
    REGISTER_TRANSFORM(TransformVariableList);

    REGISTER_TRANSFORM(TransformExpression);
    REGISTER_TRANSFORM(TransformBaseExpression);
    REGISTER_TRANSFORM(TransformRecursiveExpression);
    REGISTER_TRANSFORM(TransformSingleExpression);
    REGISTER_TRANSFORM(TransformParenthesisExpression);
    REGISTER_TRANSFORM(TransformLiteralExpression);
    REGISTER_TRANSFORM(TransformColumnReference);
    REGISTER_TRANSFORM(TransformColIdOrString);
    REGISTER_TRANSFORM(TransformIdentifierOrStringLiteral);
    REGISTER_TRANSFORM(TransformColId);
    REGISTER_TRANSFORM(TransformStringLiteral);
    REGISTER_TRANSFORM(TransformIdentifier);
    REGISTER_TRANSFORM(TransformSetSetting);
    REGISTER_TRANSFORM(TransformSetVariable);
    REGISTER_TRANSFORM(TransformDottedIdentifier);

    REGISTER_TRANSFORM(TransformOperator);

    // Manual registration for mismatched names or special cases
    Register("PragmaName", &TransformIdentifierOrKeyword);
    Register("TypeName", &TransformIdentifierOrKeyword);
    Register("ColLabel", &TransformIdentifierOrKeyword);
    Register("PlainIdentifier", &TransformIdentifierOrKeyword);
    Register("QuotedIdentifier", &TransformIdentifierOrKeyword);
    Register("ReservedKeyword", &TransformIdentifierOrKeyword);
    Register("UnreservedKeyword", &TransformIdentifierOrKeyword);
    Register("ColumnNameKeyword", &TransformIdentifierOrKeyword);
    Register("FuncNameKeyword", &TransformIdentifierOrKeyword);
    Register("TypeNameKeyword", &TransformIdentifierOrKeyword);
    Register("SettingName", &TransformIdentifierOrKeyword);

    Register("ReservedSchemaQualification", &TransformSchemaQualification);

    // Enum registration
    RegisterEnum<SetScope>("LocalScope", SetScope::LOCAL);
    RegisterEnum<SetScope>("GlobalScope", SetScope::GLOBAL);
    RegisterEnum<SetScope>("SessionScope", SetScope::SESSION);
    RegisterEnum<SetScope>("VariableScope", SetScope::VARIABLE);

    RegisterEnum<Value>("FalseLiteral", Value(false));
    RegisterEnum<Value>("TrueLiteral", Value(true));
    RegisterEnum<Value>("NullLiteral", Value());

    RegisterEnum<GenericCopyOption>("BinaryOption", GenericCopyOption("format", Value("binary")));
    RegisterEnum<GenericCopyOption>("FreezeOption", GenericCopyOption("freeze", Value()));
    RegisterEnum<GenericCopyOption>("OidsOption", GenericCopyOption("oids", Value()));
    RegisterEnum<GenericCopyOption>("CsvOption", GenericCopyOption("format", Value("csv")));
    RegisterEnum<GenericCopyOption>("HeaderOption", GenericCopyOption("header", Value()));

    RegisterEnum<TransactionModifierType>("ReadOnly", TransactionModifierType::TRANSACTION_READ_ONLY);
    RegisterEnum<TransactionModifierType>("ReadWrite", TransactionModifierType::TRANSACTION_READ_WRITE);

    RegisterEnum<CopyDatabaseType>("CopySchema", CopyDatabaseType::COPY_SCHEMA);
    RegisterEnum<CopyDatabaseType>("CopyData", CopyDatabaseType::COPY_DATA);

    RegisterEnum<LogicalType>("IntType", LogicalType(LogicalTypeId::INTEGER));
    RegisterEnum<LogicalType>("IntegerType", LogicalType(LogicalTypeId::INTEGER));
    RegisterEnum<LogicalType>("SmallintType", LogicalType(LogicalTypeId::SMALLINT));
    RegisterEnum<LogicalType>("BigintType", LogicalType(LogicalTypeId::BIGINT));
    RegisterEnum<LogicalType>("RealType", LogicalType(LogicalTypeId::FLOAT));
    RegisterEnum<LogicalType>("DoubleType", LogicalType(LogicalTypeId::DOUBLE));
    RegisterEnum<LogicalType>("BooleanType", LogicalType(LogicalTypeId::BOOLEAN));

}
"""

def extract_transformer_rules(code):
    """Extracts transformer rule names from C++ code."""
    all_rules = []
    pattern_macro = re.compile(r'REGISTER_TRANSFORM\s*\(\s*Transform([A-Za-z0-9_]+)\s*\);')
    all_rules.extend(pattern_macro.findall(code))
    pattern_register = re.compile(r'Register\s*\(\s*"([A-Za-z0-9_]+)"')
    all_rules.extend(pattern_register.findall(code))
    pattern_enum = re.compile(r'RegisterEnum<.*?>\s*\(\s*"([A-Za-z0-9_]+)"')
    all_rules.extend(pattern_enum.findall(code))
    return set(all_rules)

# --- Part 2: New logic to extract grammar rules from .gram files ---

def extract_grammar_rules(directory_path):
    """
    Extracts grammar rule names from all .gram files in a directory.
    A rule is defined as the word preceding a '<-' symbol at the start of a line.
    """
    grammar_rules = set()

    if not os.path.isdir(directory_path):
        print(f"Warning: Directory not found at '{directory_path}'")
        return grammar_rules

    # Regex to find a word at the beginning of a line followed by '<-'
    # The MULTILINE flag allows '^' to match the start of each line.
    rule_pattern = re.compile(r'^([A-Za-z0-9_]+)\s*<-', re.MULTILINE)

    for filename in os.listdir(directory_path):
        if filename.endswith(".gram"):
            filepath = os.path.join(directory_path, filename)
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                    matches = rule_pattern.findall(content)
                    grammar_rules.update(matches)
            except Exception as e:
                print(f"Error reading file {filepath}: {e}")

    return grammar_rules

# --- Main execution block ---
if __name__ == "__main__":
    # --- IMPORTANT ---
    # Change this variable to point to your actual grammar folder.
    # We will use a mock folder for this example to run.
    GRAMMAR_ROOT_DIR = "../extension/autocomplete/grammar/statements"

    print(f"--- Analyzing grammar files in: '{GRAMMAR_ROOT_DIR}' ---\n")

    # 1. Get the set of transformer rules from the C++ code
    transformer_rules = extract_transformer_rules(cpp_code)

    # 2. Get the set of grammar rules from the .gram files
    grammar_rules = extract_grammar_rules(GRAMMAR_ROOT_DIR)

    if not grammar_rules:
        print("No grammar rules found. Please check the directory path and file contents.")
    else:
        # 3. Compare the sets and report the results
        covered_rules = grammar_rules.intersection(transformer_rules)
        missing_rules = grammar_rules.difference(transformer_rules)

        # Calculate coverage
        total_grammar_rules = len(grammar_rules)
        total_covered = len(covered_rules)
        coverage_percentage = (total_covered / total_grammar_rules) * 100 if total_grammar_rules > 0 else 0

        # Print the summary report
        print("--- Grammar Transformer Coverage Report ---")
        print(f"Total grammar rules found: {total_grammar_rules}")
        print(f"Total transformer rules found: {len(transformer_rules)}")
        print(f"Covered grammar rules: {total_covered}")
        print(f"Coverage: {coverage_percentage:.2f}%\n")

        print("--- Covered Transformer Rules ---")
        for rule in sorted(list(covered_rules)):
            print(rule)
        print()

        if missing_rules:
            print("--- Missing Transformer Rules ---")
            for rule in sorted(list(missing_rules)):
                print(rule)
        else:
            print("--- All grammar rules are covered! ---")