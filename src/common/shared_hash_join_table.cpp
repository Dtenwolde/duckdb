#include "duckdb/common/shared_hash_join_table.h"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

SharedJoinTable::SharedJoinTable(bool found) : found(found) {

}


} // namespace duckdb