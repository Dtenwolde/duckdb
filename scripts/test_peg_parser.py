import argparse
import os
import sqllogictest
from sqllogictest import SQLParserException, SQLLogicParser, SQLLogicTest
import subprocess
import multiprocessing
import tempfile
import re
import requests
from urllib.parse import urlparse
from typing import Optional
import csv
from datetime import datetime
from collections import Counter, defaultdict

# --- Argument Parsing Setup ---
parser = argparse.ArgumentParser(description="Test the PEG parser and transformer.")
parser.add_argument("--shell", type=str, help="Shell binary to run", default=os.path.join('build', 'debug', 'duckdb'))
parser.add_argument("--offset", type=int, help="File offset", default=None)
parser.add_argument("--count", type=int, help="File count", default=None)
parser.add_argument('--no-exit', action='store_true', help='Do not exit after a test fails', default=False)
parser.add_argument('--print-failing-only', action='store_true', help='Print failing tests only', default=False)
parser.add_argument(
    '--include-extensions', action='store_true', help='Include test files of out-of-tree extensions', default=False
)

# New arguments for mode and statement type
parser.add_argument(
    '--mode',
    type=str,
    choices=['parser', 'transformer'],
    default='parser',
    help='Specify the testing mode: "parser" (only parsing) or "transformer" (parsing + transforming).',
)
parser.add_argument(
    '--statement-type',
    type=str,
    help='Filter tests to only run a specific statement type (e.g., SELECT, CREATE, INSERT). Case-insensitive.',
    default=None,
)

group = parser.add_mutually_exclusive_group(required=True)
group.add_argument("--test-file", type=str, help="Path to the SQL logic file", default='')
group.add_argument(
    "--test-list", type=str, help="Path to the file that contains a newline separated list of test files", default=''
)
group.add_argument("--all-tests", action='store_true', help="Run all tests", default=False)
args = parser.parse_args()


def extract_git_urls(script: str):
    pattern = r'GIT_URL\s+(https?://\S+)'
    return re.findall(pattern, script)


def download_directory_contents(api_url, local_path, headers):
    response = requests.get(api_url, headers=headers)
    if response.status_code != 200:
        print(f"⚠️  Could not access {api_url}: {response.status_code}")
        return

    os.makedirs(local_path, exist_ok=True)

    for item in response.json():
        item_type = item.get("type")
        item_name = item.get("name")
        if item_type == "file":
            download_url = item.get("download_url")
            if not download_url:
                continue
            file_path = os.path.join(local_path, item_name)
            file_resp = requests.get(download_url)
            if file_resp.status_code == 200:
                with open(file_path, "wb") as f:
                    f.write(file_resp.content)
                print(f"  - Downloaded {file_path}")
            else:
                print(f"  - Failed to download {file_path}")
        elif item_type == "dir":
            subdir_api_url = item.get("url")
            subdir_local_path = os.path.join(local_path, item_name)
            download_directory_contents(subdir_api_url, subdir_local_path, headers)


def download_test_sql_folder(repo_url, base_folder="extension-test-files"):
    repo_name = urlparse(repo_url).path.strip("/").split("/")[-1]
    target_folder = os.path.join(base_folder, repo_name)

    if os.path.exists(target_folder):
        print(f"✓ Skipping {repo_name}, already exists.")
        return

    print(f"⬇️ Downloading test/sql from {repo_name}...")

    api_url = f"https://api.github.com/repos/duckdb/{repo_name}/contents/test/sql?ref=main"
    GITHUB_TOKEN = os.environ.get("GITHUB_TOKEN")
    if not GITHUB_TOKEN:
        raise ValueError("GITHUB_TOKEN environment variable is not set.")
    headers = {"Accept": "application/vnd.github.v3+json", "Authorization": f"Bearer {GITHUB_TOKEN}"}

    download_directory_contents(api_url, target_folder, headers)


def batch_download_all_test_sql():
    filename = ".github/config/out_of_tree_extensions.cmake"
    if not os.path.isfile(filename):
        raise Exception(f"File {filename} not found")
    with open(filename, "r") as f:
        content = f.read()
    urls = extract_git_urls(content)
    if not urls:
        print("No URLs found.")
    for url in urls:
        download_test_sql_folder(url)


def find_tests_recursive(dir, excluded_paths):
    test_list = []
    for f in os.listdir(dir):
        path = os.path.join(dir, f)
        if path in excluded_paths:
            continue
        if os.path.isdir(path):
            test_list += find_tests_recursive(path, excluded_paths)
        elif path.endswith('.test') or path.endswith('.test_slow'):
            test_list.append(path)
    return test_list


def parse_test_file(filename):
    if not os.path.isfile(filename):
        raise Exception(f"File {filename} not found")
    parser = SQLLogicParser()
    try:
        out: Optional[SQLLogicTest] = parser.parse(filename)
        if not out:
            raise SQLParserException(f"Test {filename} could not be parsed")
    except Exception:
        return []
    loop_count = 0
    statements = []
    for stmt in out.statements:
        if isinstance(stmt, sqllogictest.statement.skip.Skip):
            break
        if isinstance(stmt, (sqllogictest.statement.loop.Loop, sqllogictest.statement.foreach.Foreach)):
            loop_count += 1
        if isinstance(stmt, sqllogictest.statement.endloop.Endloop):
            loop_count -= 1
        if loop_count > 0:
            continue
        if not isinstance(stmt, (sqllogictest.statement.query.Query, sqllogictest.statement.statement.Statement)):
            continue
        if isinstance(stmt, sqllogictest.statement.statement.Statement):
            if stmt.expected_result.type == sqllogictest.ExpectedResult.Type.ERROR:
                if any("parser error" in line.lower() or "syntax error" in line.lower() for line in stmt.expected_result.lines):
                    continue
                continue
        query = ' '.join(stmt.lines)
        statements.append(query)
    return statements


def run_test_case(args_tuple):
    i, file, shell, print_failing_only, mode, statement_type = args_tuple
    results = []
    if not print_failing_only:
        print(f"Run test {i}: {file} (mode: {mode})")

    all_statements = parse_test_file(file)

    # --- Filter statements by type if requested ---
    statements_to_run = []
    if statement_type:
        for stmt in all_statements:
            clean_stmt = stmt.strip()
            if not clean_stmt:
                continue
            # Simple check of the first word to determine statement type
            first_word = clean_stmt.split()[0].upper()
            if first_word == statement_type.upper():
                statements_to_run.append(stmt)
    else:
        statements_to_run = all_statements

    if not statements_to_run:
        return []

    # --- Select the appropriate test function based on the mode ---
    if mode == 'transformer':
        # Assuming the new function is named 'check_peg_transformer'
        test_function = 'check_peg_transformer'
    else:
        test_function = 'check_peg_parser'

    for statement in statements_to_run:
        with tempfile.TemporaryDirectory() as tmpdir:
            peg_sql_path = os.path.join(tmpdir, 'peg_test.sql')
            with open(peg_sql_path, 'w') as f:
                # Use the selected test function
                f.write(f'CALL {test_function}($TEST_PEG_PARSER${statement}$TEST_PEG_PARSER$);\n')

            proc = subprocess.run([shell, '-init', peg_sql_path, '-c', '.exit'], capture_output=True)
            stderr = proc.stderr.decode('utf8')

            if proc.returncode == 0 and ' Error:' not in stderr:
                continue

            if print_failing_only:
                print(f"Failed test {i}: {file}")
            else:
                print(f'Failed')
                print(f'-- STDOUT --')
                print(proc.stdout.decode('utf8'))
                print(f'-- STDERR --')
                print(stderr)

            results.append((file, statement, stderr))
            break
    return results


if __name__ == "__main__":
    files = []
    excluded_tests = {
        'test/sql/peg_parser',  # Fail for some reason
        'test/sql/prepared/parameter_variants.test',  # PostgreSQL parser bug with ?1
        'test/sql/copy/s3/download_config.test',  # Unknown why this passes in SQLLogicTest
    }
    if args.all_tests:
        test_dir = os.path.join('test', 'sql')
        files = find_tests_recursive(test_dir, excluded_tests)
        if args.include_extensions:
            batch_download_all_test_sql()
            extension_files = find_tests_recursive('extension-test-files', {})
            files += extension_files
    elif len(args.test_list) > 0:
        with open(args.test_list, 'r') as f:
            files = [x.strip() for x in f.readlines() if x.strip() not in excluded_tests]
    else:
        files.append(args.test_file)
    files.sort()

    start = args.offset if args.offset is not None else 0
    end = start + args.count if args.count is not None else len(files)

    # --- Pass new arguments to the test runner ---
    work_items = [
        (i, files[i], args.shell, args.print_failing_only, args.mode, args.statement_type) for i in range(start, end)
    ]

    failed_test_list = []
    if not args.no_exit:
        for item in work_items:
            res = run_test_case(item)
            if res:
                failed_test_list.extend(res)
                exit(1)
    else:
        with multiprocessing.Pool() as pool:
            results = pool.map(run_test_case, work_items)
        failed_test_list = [item for sublist in results for item in sublist]

    failed_tests = len(failed_test_list)
    total_tests_run = len(files)
    error_types = defaultdict(int)
    error_messages = defaultdict(list)
    for _, statement, std_err_ in failed_test_list:
        first_line_of_error = std_err_.splitlines()[0]
        type_of_error = first_line_of_error.split(":")
        error_messages[first_line_of_error].append(statement)
        error_types[type_of_error[0]] += 1
    sorted_errors = sorted(list((k, len(v), v) for k,v in error_messages.items()), key=lambda t:t[1], reverse=True)[:10]
    if total_tests_run > 0:
        # --- This part remains the same: print detailed failures to console ---
        print("\nList of failed tests: ")
        for test, statement, stderr_ in failed_test_list:
            print(f"{test}\n{statement}\n{stderr_}\n\n")

        print("--- Error Message Frequency ---")
        for message, count, queries in sorted_errors:
            print(f"Count: {count} | Message: \"{message.strip()}\"")
            print(f"Example Query: {queries[0]}\n\n")

        percentage_failed = round(failed_tests / total_tests_run * 100, 2)
        print(f"Total of {failed_tests} out of {total_tests_run} failed ({percentage_failed}%).\n")

        print("--- Error Message Type ---")
        for error_type, count in error_types.items():
            print(f"Type: {error_type} | Count: {count}")

        # --- New section to append summary to a single CSV file ---
        try:
            filename = "test_summary.csv"
            now = datetime.now()

            # 1. Check if the file already exists. We'll use this to decide whether to write the header.
            file_exists = os.path.exists(filename)

            # 2. Define the header and data row for the CSV
            header = ['timestamp', 'failed_tests', 'total_tests', 'percentage_failed']
            data_row = [now.isoformat(), failed_tests, total_tests_run, percentage_failed]

            # 3. Open the file in append mode ('a')
            with open(filename, 'a', newline='') as csvfile:
                writer = csv.writer(csvfile)

                # Write the header only if the file is newly created
                if not file_exists:
                    writer.writerow(header)

                # Always write the data row
                writer.writerow(data_row)

            print(f"\nTest summary appended to {filename}")

        except IOError as e:
            print(f"\nError: Could not write summary to file {filename}. Reason: {e}")

    else:
        print("No tests were run.")