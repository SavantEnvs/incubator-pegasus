// Minimal runtime glue. Logging: a minimal sink for the dsn logging entry points declared in src/utils/api_utilities.h.
// The real implementation (src/utils/logging.cpp) pulls in the whole rDSN runtime
// (tool_api, service engine, thrift RPC types). The harnesses only fuzz pure parsers,
// so log lines are dropped and a FATAL log (what CHECK/CHECK_* expand to) aborts, as
// upstream's dsn_coredump() does.
#include <cstdio>
#include <cstdlib>
#include "utils/api_utilities.h"

log_level_t log_start_level = LOG_LEVEL_FATAL;
log_level_t get_log_start_level() { return log_start_level; }
void set_log_start_level(log_level_t level) { log_start_level = level; }
void dsn_coredump() { abort(); }
void global_log(const char *file, const char *function, const int line, log_level_t log_level,
                const char *str)
{
    if (log_level >= LOG_LEVEL_FATAL) {
        fprintf(stderr, "FATAL %s:%d %s: %s\n", file, line, function, str);
        dsn_coredump();
    }
}

// task_spec.cpp (linked for task-code registration) reads [network].enable_udp, which
// upstream defines in src/runtime/providers.common.cpp next to every runtime provider.
// Same definition and default as upstream.
#include "utils/flags.h"
DSN_DEFINE_bool(network, enable_udp, true, "whether to enable udp rpc engine");

// rpc_host_port.cpp's host_port::resolve() forwards to the runtime's dns_resolver
// (src/rpc/dns_resolver.cpp), which needs the metrics/service-engine runtime. No harness
// calls resolve(); the sanitizer-coverage sections keep it from being garbage-collected,
// so the two entry points it references are provided here and abort if ever reached.
#include "rpc/dns_resolver.h"
namespace dsn {
dns_resolver::dns_resolver() { abort(); }
rpc_address dns_resolver::resolve_address(const host_port &) { abort(); }
} // namespace dsn
