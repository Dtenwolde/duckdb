#include "catch.hpp"
#include "test_helpers.hpp"

#include <fcntl.h>
#include <string>
#include <unistd.h>

using namespace duckdb;
using namespace std;

// Test that summarize works correctly when the input is a pipe (read-once file).
// Regression test: BindSummarize binds the query twice; the second bind used to
// create a fresh CSVBufferManager on the exhausted pipe, yielding 0 rows.
TEST_CASE("Test summarize with piped CSV source", "[csv][pipe][summarize]") {
	// Create a POSIX pipe and fill it with CSV data before DuckDB opens it.
	int pipe_fds[2];
	REQUIRE(::pipe(pipe_fds) == 0);

	// Write "1\n2\n" to the write end, then close it so DuckDB sees EOF.
	const char *csv_data = "1\n2\n";
	REQUIRE(::write(pipe_fds[1], csv_data, strlen(csv_data)) == (ssize_t)strlen(csv_data));
	::close(pipe_fds[1]);

	// Build the /dev/fd/N path so DuckDB can open the read end by path.
	string pipe_path = "/dev/fd/" + to_string(pipe_fds[0]);

	DuckDB db(nullptr);
	Connection con(db);

	string query = "SELECT count, min, max FROM "
	               "(SUMMARIZE (FROM read_csv('" +
	               pipe_path + "', columns = {'v': 'DOUBLE'})))";

	auto result = con.Query(query);
	::close(pipe_fds[0]);

	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->RowCount() == 1);

	auto count = result->GetValue(0, 0); // count
	REQUIRE(count == Value::BIGINT(2));

	auto min_val = result->GetValue(1, 0); // min
	REQUIRE(min_val == Value("1.0"));

	auto max_val = result->GetValue(2, 0); // max
	REQUIRE(max_val == Value("2.0"));
}
