// Fuzzes Pegasus' rapidjson-based metadata decoding (src/common/json_helper.h,
// dsn::json::json_forwarder<T>::decode) for the types it defines decoders for:
// app_info (table metadata read back from the meta store / cold-backup restore),
// partition_configuration (meta store), and bulk_load_metadata (bulk-load files fetched
// from remote block storage). The first byte selects the type.
#include <cstdint>
#include <string>

#include "common/json_helper.h"
#include "utils/blob.h"

template <typename T>
static void decode_and_reencode(const dsn::blob &b)
{
    T obj;
    if (dsn::json::json_forwarder<T>::decode(b, obj)) {
        dsn::blob out = dsn::json::json_forwarder<T>::encode(obj);
        T again;
        (void)dsn::json::json_forwarder<T>::decode(out, again);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    std::string s(reinterpret_cast<const char *>(data + 1), size - 1);
    dsn::blob b = dsn::blob::create_from_bytes(std::move(s));
    switch (data[0] % 3) {
    case 0:
        decode_and_reencode<dsn::app_info>(b);
        break;
    case 1:
        decode_and_reencode<dsn::partition_configuration>(b);
        break;
    default:
        decode_and_reencode<dsn::replication::bulk_load_metadata>(b);
        break;
    }
    return 0;
}
