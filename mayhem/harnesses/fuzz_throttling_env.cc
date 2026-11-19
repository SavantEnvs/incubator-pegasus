// Fuzzes the app-env value parsers for write/read throttling (set remotely with
// `set_app_envs`): throttling_controller::parse_from_env (e.g. "20000*delay*100,20000*reject*100")
// and token_bucket_throttling_controller::validate / parse_from_env (e.g. "200K*delay*100").
#include <cstdint>
#include <string>

#include "utils/throttling_controller.h"
#include "utils/token_bucket_throttling_controller.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    // partition_count > 0 (callers pass the table's partition count)
    int partition_count = (data[0] % 64) + 1;
    std::string env(reinterpret_cast<const char *>(data + 1), size - 1);

    std::string hint, old_env;
    bool changed = false;
    {
        dsn::utils::throttling_controller c;
        if (c.parse_from_env(env, partition_count, hint, changed, old_env)) {
            int64_t delay_ms = 0;
            (void)c.control(1000, 1, delay_ms);
            (void)c.parse_from_env(env, partition_count, hint, changed, old_env);
        }
        uint64_t du, dm, ru, rm;
        (void)dsn::utils::throttling_controller::parse_from_env(env, du, dm, ru, rm, hint);
    }
    {
        (void)dsn::utils::token_bucket_throttling_controller::validate(env, hint);
        dsn::utils::token_bucket_throttling_controller t;
        (void)t.parse_from_env(env, partition_count, hint, changed, old_env);
    }
    return 0;
}
