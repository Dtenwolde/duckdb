import os
import re
from collections import defaultdict

# --- Configuration ---
# IMPORTANT: Adjust these paths to match your project structure.
GRAMMAR_DIR = "../extension/autocomplete/grammar/statements"
HEADER_FILE = "../extension/autocomplete/transformer/peg_transformer.hpp"
FACTORY_FILE = "../extension/autocomplete/transformer/peg_transformer_factory.cpp"
SOURCE_DIR = "../extension/autocomplete/transformer"

# --- Part 1: Functions to extract existing rules ---

def extract_transformer_rules_from_string(code_content):
    """Extracts implemented transformer rule names from the factory C++ code."""
    all_rules = set()
    if not code_content:
        return all_rules

    pattern_macro = re.compile(r'REGISTER_TRANSFORM\s*\(\s*Transform([A-Za-z0-9_]+)\s*\);')
    all_rules.update(pattern_macro.findall(code_content))
    pattern_register = re.compile(r'Register\s*\(\s*"([A-Za-z0-9_]+)"')
    all_rules.update(pattern_register.findall(code_content))
    pattern_enum = re.compile(r'RegisterEnum<.*?>\s*\(\s*"([A-Za-z0-9_]+)"')
    all_rules.update(pattern_enum.findall(code_content))
    return all_rules

def extract_grammar_rules_from_dir(directory_path):
    """Extracts all grammar rule names from .gram files, mapping them back to their source file."""
    rules_to_file_map = {}
    if not os.path.isdir(directory_path):
        print(f"Warning: Grammar directory not found at '{directory_path}'")
        return rules_to_file_map

    rule_pattern = re.compile(r'^([A-Za-z0-9_]+)\s*<-', re.MULTILINE)
    for filename in sorted(os.listdir(directory_path)):
        if filename.endswith(".gram"):
            filepath = os.path.join(directory_path, filename)
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                    matches = rule_pattern.findall(content)
                    for match in matches:
                        rules_to_file_map[match] = filename
            except Exception as e:
                print(f"Error reading file {filepath}: {e}")
    return rules_to_file_map

# --- Part 2: Main execution ---

if __name__ == "__main__":
    print("--- Running Transformer Stub Generator ---")

    # 1. Get implemented rules from the C++ factory file
    try:
        with open(FACTORY_FILE, 'r', encoding='utf-8') as f:
            cpp_content = f.read()
        implemented_rules = extract_transformer_rules_from_string(cpp_content)
    except FileNotFoundError:
        print(f"\nFATAL: Factory file not found at '{FACTORY_FILE}'. Exiting.")
        exit(1)

    # 2. Get all grammar rules from the .gram files
    all_grammar_rules_map = extract_grammar_rules_from_dir(GRAMMAR_DIR)
    all_grammar_rules = set(all_grammar_rules_map.keys())

    # 3. Find the missing rules
    missing_rules = sorted(list(all_grammar_rules - implemented_rules))

    if not missing_rules:
        print("\nSUCCESS: All grammar rules have a corresponding transformer registration. Nothing to do.")
        exit(0)

    print(f"\nFound {len(missing_rules)} missing transformer functions. Generating stubs...\n")

    # 4. Generate the code snippets and write them to a file, grouped by rule
    OUTPUT_FILENAME = "transformer_stubs.txt"
    try:
        with open(OUTPUT_FILENAME, "w", encoding="utf-8") as f:
            f.write(f"--- Generated Stubs for {len(missing_rules)} Missing Transformers ---\n")

            for rule_name in missing_rules:
                transform_func_name = f"Transform{rule_name}"
                source_gram_file = all_grammar_rules_map.get(rule_name, "unknown.gram")
                target_cpp_file = f"transform_{source_gram_file.replace('.gram', '.cpp')}"

                # Generate snippets for the current rule
                header_snippet = f"static string {transform_func_name}(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);"
                factory_snippet = f"    REGISTER_TRANSFORM({transform_func_name});"
                source_body = (f'string PEGTransformerFactory::{transform_func_name}(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {{\n'
                               f'    auto &list_pr = parse_result->Cast<ListParseResult>();\n'
                               f'    throw NotImplementedException("Rule \'{rule_name}\' has not been implemented yet");\n'
                               f'}}')

                # Write the grouped output for this rule
                f.write("\n" + "="*60 + "\n")
                f.write(f"--- Rule: {rule_name} ---\n")
                f.write("="*60 + "\n\n")

                f.write(f"// 1. Add to header file: '{HEADER_FILE}'\n")
                f.write(header_snippet + "\n\n")

                f.write(f"// 2. Add to factory constructor: '{FACTORY_FILE}'\n")
                f.write(factory_snippet + "\n\n")

                f.write(f"// 3. Add to source file: '{os.path.join(SOURCE_DIR, target_cpp_file)}'\n")
                f.write(source_body + "\n")

        print(f"SUCCESS: Generated stubs have been written to '{OUTPUT_FILENAME}'")

    except Exception as e:
        print(f"Error writing to output file: {e}")

