import os
import subprocess

# File paths
sql_peg_file = 'sql.peg'
reserved_keywords_file = 'keywords/reserved_keywords.list'
unreserved_keywords_file = 'keywords/unreserved_keywords.list'
output_grammar_file = 'sql.gram'
peglint_source_file = 'peglint.cc'
peglint_executable = 'peglint'

def read_keywords(file_path):
    """Read keywords from a file and return a set of keywords."""
    with open(file_path, 'r') as file:
        return {line.strip() for line in file if line.strip()}

def verify_keywords(reserved_keywords, unreserved_keywords):
    """Verify that there are no overlapping keywords and prune any ending with _P."""
    # Check for overlapping keywords
    common_keywords = reserved_keywords & unreserved_keywords
    if common_keywords:
        raise ValueError(f"Overlapping keywords found: {common_keywords}")

    # Prune keywords ending with _P by removing the _P suffix
    reserved_keywords = {kw[:-2] if kw.endswith('_P') else kw for kw in reserved_keywords}
    unreserved_keywords = {kw[:-2] if kw.endswith('_P') else kw for kw in unreserved_keywords}

    return reserved_keywords, unreserved_keywords
def generate_grammar(input_peg, output_gram, reserved_keywords, unreserved_keywords):
    """Generate the output grammar file with additional rules for ReservedKeyword and UnreservedKeyword."""
    with open(input_peg, 'r') as infile, open(output_gram, 'w') as outfile:
        # Copy the original content of sql.peg
        for line in infile:
            outfile.write(line)

        # Add ReservedKeyword rule
        outfile.write("\n\nReservedKeyword <-\n")
        reserved_rules = [f"    '{kw.upper()}'i" for kw in reserved_keywords]
        outfile.write("    " + " /\n    ".join(reserved_rules) + "\n")

        # Add UnreservedKeyword rule
        outfile.write("\nUnreservedKeyword <-\n")
        unreserved_rules = [f"    '{kw.upper()}'i" for kw in unreserved_keywords]
        outfile.write("    " + " /\n    ".join(unreserved_rules) + "\n")

def compile_peglint(peglint_source, output_executable):
    """Compile peglint.cc to create an executable."""
    compile_command = ['g++', '-std=c++17', peglint_source, '-o', output_executable, '-I.']
    result = subprocess.run(compile_command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Failed to compile peglint: {result.stderr}")

def run_peglint(grammar_file, peglint_exec):
    """Run the peglint executable to verify the grammar file."""
    lint_command = [peglint_exec, grammar_file]
    result = subprocess.run(lint_command, capture_output=True, text=True)

    # Output whatever peglint returns
    print(result.stdout)
    if result.stderr:
        print(result.stderr)

    # Check the return code to determine if it passed or failed
    if result.returncode != 0:
        print("Peglint encountered errors.")
        return False
    else:
        print("Grammar verification passed with no errors.")
        return True

def main():
    # Step 1: Read keywords from files
    reserved_keywords = read_keywords(reserved_keywords_file)
    unreserved_keywords = read_keywords(unreserved_keywords_file)

    # Step 2: Verify keywords
    try:
        reserved_keywords, unreserved_keywords = verify_keywords(reserved_keywords, unreserved_keywords)
    except ValueError as e:
        print(f"Keyword verification failed: {e}")
        return

    # Step 3: Generate the output grammar file with ReservedKeyword and UnreservedKeyword rules
    generate_grammar(sql_peg_file, output_grammar_file, reserved_keywords, unreserved_keywords)

    # Step 4: Compile the peglint tool
    try:
        compile_peglint(peglint_source_file, peglint_executable)
    except RuntimeError as e:
        print(f"Compilation of peglint failed: {e}")
        return

    # Step 5: Run peglint to check the grammar file
    if run_peglint(output_grammar_file, peglint_executable):
        print("All clear: The grammar file is correct.")
    else:
        print("Grammar verification failed.")

if __name__ == '__main__':
    main()