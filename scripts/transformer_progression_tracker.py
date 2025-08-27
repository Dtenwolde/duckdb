import os
import re

# --- Part 1: Functions to extract rules from files ---

def extract_transformer_rules_from_string(code_content):
    """Extracts transformer rule names from a string of C++ code."""
    all_rules = set()
    if not code_content:
        return all_rules

    # Pattern 1: REGISTER_TRANSFORM(TransformRuleName)
    pattern_macro = re.compile(r'REGISTER_TRANSFORM\s*\(\s*Transform([A-Za-z0-9_]+)\s*\);')
    all_rules.update(pattern_macro.findall(code_content))

    # Pattern 2: Register("RuleName", ...)
    pattern_register = re.compile(r'Register\s*\(\s*"([A-Za-z0-9_]+)"')
    all_rules.update(pattern_register.findall(code_content))

    # Pattern 3: RegisterEnum<...>("RuleName", ...)
    pattern_enum = re.compile(r'RegisterEnum<.*?>\s*\(\s*"([A-Za-z0-9_]+)"')
    all_rules.update(pattern_enum.findall(code_content))

    return all_rules

def extract_grammar_rules_from_dir(directory_path):
    """
    Extracts grammar rule names from all .gram files in a directory.
    Returns a dictionary mapping filename to a set of rules.
    """
    grammar_rules_per_file = {}

    if not os.path.isdir(directory_path):
        print(f"Warning: Grammar directory not found at '{directory_path}'")
        return grammar_rules_per_file

    # Regex to find a word at the beginning of a line followed by '<-'
    rule_pattern = re.compile(r'^([A-Za-z0-9_]+)\s*<-', re.MULTILINE)

    for filename in sorted(os.listdir(directory_path)):
        if filename.endswith(".gram"):
            filepath = os.path.join(directory_path, filename)
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                    matches = rule_pattern.findall(content)
                    if matches:
                        grammar_rules_per_file[filename] = set(matches)
            except Exception as e:
                print(f"Error reading file {filepath}: {e}")

    return grammar_rules_per_file

# --- Part 2: Mock environment setup for demonstration ---

def setup_mock_environment(base_dir, cpp_factory_path):
    """Creates a fake directory structure and files for demonstration."""
    # This setup is unchanged but is used by the new reporting logic
    statements_dir = os.path.join(base_dir, "statements")
    os.makedirs(statements_dir, exist_ok=True)
    with open(os.path.join(statements_dir, "create.gram"), "w") as f:
        f.write("CreateStatement <- 'CREATE'\nCreateStatementVariation <- CreateTableStmt")
    with open(os.path.join(statements_dir, "copy.gram"), "w") as f:
        f.write("CopyStatement <- 'COPY'\nCopyTable <- QualifiedName")
    with open(os.path.join(statements_dir, "analyze.gram"), "w") as f:
        f.write("AnalyzeStatement <- 'ANALYZE'")

    cpp_dir = os.path.dirname(cpp_factory_path)
    os.makedirs(cpp_dir, exist_ok=True)
    with open(cpp_factory_path, "w") as f:
        f.write("""
        PEGTransformerFactory::PEGTransformerFactory() {
            REGISTER_TRANSFORM(TransformCreateStatement);
            REGISTER_TRANSFORM(TransformCopyStatement);
            Register("CopyTable", &TransformQualifiedName);
        }
        """)
    return statements_dir

# --- Part 3: Main execution block ---

if __name__ == "__main__":
    # Change these variables to point to your actual project files and folders.
    GRAMMAR_ROOT_DIR = "../extension/autocomplete/grammar/statements"
    CPP_FACTORY_FILE = "../extension/autocomplete/transformer/peg_transformer_factory.cpp"

    print(f"--- Analyzing grammar files in: '{GRAMMAR_ROOT_DIR}' ---")
    print(f"--- Analyzing C++ file: '{CPP_FACTORY_FILE}' ---\n")

    # 1. Get the global set of transformer rules from the C++ file
    try:
        with open(CPP_FACTORY_FILE, 'r', encoding='utf-8') as f:
            cpp_content = f.read()
        transformer_rules = extract_transformer_rules_from_string(cpp_content)
    except FileNotFoundError:
        print(f"FATAL: C++ factory file not found at '{CPP_FACTORY_FILE}'. Exiting.")
        exit(1)
    except Exception as e:
        print(f"Error reading {CPP_FACTORY_FILE}: {e}")
        transformer_rules = set()

    # 2. Get grammar rules, now organized per file
    all_grammar_rules_per_file = extract_grammar_rules_from_dir(GRAMMAR_ROOT_DIR)

    if not all_grammar_rules_per_file:
        print("No grammar files with rules found. Please check the directory path.")
    else:
        # --- Per-File Reporting ---
        print("--- Per-File Coverage Details ---")
        all_grammar_rules = set()

        for filename, rules_in_file in all_grammar_rules_per_file.items():
            all_grammar_rules.update(rules_in_file)

            covered_in_file = rules_in_file.intersection(transformer_rules)
            missing_in_file = rules_in_file.difference(transformer_rules)

            total_in_file = len(rules_in_file)
            coverage_perc = (len(covered_in_file) / total_in_file) * 100 if total_in_file > 0 else 0

            print("-" * 40)
            print(f"File: {filename}")
            print(f"Coverage: {coverage_perc:.2f}% ({len(covered_in_file)} / {total_in_file} rules)")

            if missing_in_file:
                print("Missing rules:")
                for rule in sorted(list(missing_in_file)):
                    print(f"  - {rule}")
            else:
                print("Missing rules: None")
        print("-" * 40)

        # --- Total Statistics Reporting ---
        total_covered = all_grammar_rules.intersection(transformer_rules)
        total_missing = all_grammar_rules.difference(transformer_rules)

        total_grammar_count = len(all_grammar_rules)
        total_coverage_perc = (len(total_covered) / total_grammar_count) * 100 if total_grammar_count > 0 else 0

        print("\n--- Overall Summary ---")
        print(f"Total unique grammar rules found: {total_grammar_count}")
        print(f"Total unique transformer rules found: {len(transformer_rules)}")
        print(f"Total covered grammar rules: {len(total_covered)}")
        print(f"Overall Coverage: {total_coverage_perc:.2f}%\n")

        if total_missing:
            print("--- All Missing Transformer Rules (Across All Files) ---")
            for rule in sorted(list(total_missing)):
                print(rule)
        else:
            print("--- All grammar rules are covered! ---")