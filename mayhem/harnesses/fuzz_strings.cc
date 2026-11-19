// Fuzzes the string parsers in src/utils that turn remotely supplied text (remote
// commands, app envs, http query args) into values: split_args, parse_kv_map,
// pattern_match, trim_string, buf2* numeric/bool conversion, hh_mm_to_seconds.
#include <cstdint>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "utils/string_conv.h"
#include "utils/strings.h"
#include "utils/time_utils.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2)
        return -1;
    const char sep = static_cast<char>(data[0]);
    const char kv_sep = static_cast<char>(data[1]);
    std::string s(reinterpret_cast<const char *>(data + 2), size - 2);
    const char *cs = s.c_str(); // C-string APIs see the prefix up to the first NUL

    std::vector<std::string> vec;
    dsn::utils::split_args(cs, vec, sep);
    std::vector<std::string> vec2;
    dsn::utils::split_args(cs, vec2, sep, true);
    std::list<std::string> lst;
    dsn::utils::split_args(cs, lst, sep);
    std::unordered_set<std::string> st;
    dsn::utils::split_args(cs, st, sep);

    std::map<std::string, std::string> kv;
    if (dsn::utils::parse_kv_map(cs, kv, sep, kv_sep))
        (void)dsn::utils::kv_map_to_string(kv, sep, kv_sep);
    std::map<std::string, std::string> kv2;
    (void)dsn::utils::parse_kv_map(cs, kv2, sep, kv_sep, true);

    if (!vec.empty())
        (void)dsn::utils::pattern_match(
            s, vec.front(), static_cast<dsn::utils::pattern_match_type::type>(data[0] % 8));
    std::string copy(cs);
    (void)dsn::utils::trim_string(&copy[0]);
    (void)dsn::utils::get_last_component(s, std::string(1, sep));
    (void)dsn::utils::find_string_prefix(s, sep);
    (void)dsn::utils::replace_string(s, std::string(1, sep), std::string(1, kv_sep));

    int32_t i32;
    int64_t i64;
    uint32_t u32;
    uint64_t u64;
    uint16_t u16;
    bool b;
    double d;
    (void)dsn::buf2int32(s, i32);
    (void)dsn::buf2int64(s, i64);
    (void)dsn::buf2uint32(s, u32);
    (void)dsn::buf2uint64(s, u64);
    (void)dsn::buf2uint16(s, u16);
    (void)dsn::buf2bool(s, b);
    (void)dsn::buf2double(s, d);
    (void)dsn::utils::hh_mm_to_seconds(s);
    return 0;
}
