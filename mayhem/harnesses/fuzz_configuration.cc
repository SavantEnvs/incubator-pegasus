// Fuzzes dsn::configuration::load (src/utils/configuration.cpp): the ini parser for
// config.ini files the servers and tools read (sections, keys, %var% substitution,
// duplicate detection), then enumerates and reads back every value.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "utils/configuration.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Optional "%name%" argument string: everything after the first NUL byte.
    std::string body(reinterpret_cast<const char *>(data), size);
    std::string args;
    size_t nul = body.find('\0');
    if (nul != std::string::npos) {
        args = body.substr(nul + 1);
        body.resize(nul);
        if (args.find('\0') != std::string::npos)
            return -1; // arguments are a C string
    }

    const char *tmp = getenv("TMPDIR");
    std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/fuzz_config_XXXXXX";
    int fd = mkstemp(&path[0]);
    if (fd < 0)
        return 0;
    if (write(fd, body.data(), body.size()) != (ssize_t)body.size()) {
        close(fd);
        unlink(path.c_str());
        return 0;
    }
    close(fd);

    {
        dsn::configuration c;
        if (c.load(path.c_str(), args.empty() ? nullptr : args.c_str())) {
            std::vector<std::string> sections;
            c.get_all_sections(sections);
            for (const auto &s : sections) {
                std::vector<const char *> keys;
                c.get_all_keys(s.c_str(), keys);
                for (const char *k : keys) {
                    (void)c.get_value<std::string>(s.c_str(), k, "", "");
                    (void)c.get_value<int64_t>(s.c_str(), k, 0, "");
                    (void)c.get_value<uint64_t>(s.c_str(), k, 0, "");
                    (void)c.get_value<double>(s.c_str(), k, 0.0, "");
                    (void)c.get_value<bool>(s.c_str(), k, false, "");
                }
            }
        }
    }
    unlink(path.c_str());
    return 0;
}
