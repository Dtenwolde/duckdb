//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/csv_scanner/csv_pipe_manager.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unordered_map.hpp"
#include "duckdb/execution/operator/csv_scanner/csv_buffer_manager.hpp"
#include "duckdb/main/client_context_state.hpp"

namespace duckdb {

//! Caches CSVBufferManagers for pipe/stdin files between successive binds of the same pipe.
//! A pipe can only be read once; when BindSummarize binds a query twice, the second bind
//! would otherwise find the pipe exhausted. This manager lets the second bind reuse the
//! buffer (with the cached data) that was created by the first bind.
class CSVPipeManager : public ClientContextState {
public:
	void QueryEnd() override {
		buffer_managers.clear();
	}

	//! Map of file path -> cached CSVBufferManager for pipe files
	unordered_map<string, shared_ptr<CSVBufferManager>> buffer_managers;
};

} // namespace duckdb
