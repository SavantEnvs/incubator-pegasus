// Fuzzes the address parsers in src/rpc (area of upstream fix 1fbfd47, "crash caused by
// un-resolved IP address"): host_port::from_string, rpc_address::from_ip_port /
// from_host_port, host_port <-> rpc_address conversion incl. host_port::from_address,
// and the thrift wire decoding of rpc_address / host_port (rpc_address::read,
// host_port::read) used for every RPC that carries a node address.
// Offline, hostname resolution fails quickly; only numeric hosts resolve.
#include <cstdint>
#include <boost/shared_ptr.hpp>
#include <memory>
#include <string>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include "rpc/rpc_address.h"
#include "rpc/rpc_host_port.h"
#include "common/serialization_helper/thrift_helper.h" // rpc_address::read, host_port::read

using apache::thrift::protocol::TBinaryProtocol;
using apache::thrift::transport::TMemoryBuffer;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    const uint8_t mode = data[0];
    const uint8_t *p = data + 1;
    const size_t n = size - 1;

    if (mode & 1) {
        // thrift-binary encoded address structs, as received in RPC bodies
        try {
            // thrift 0.9.3 takes boost::shared_ptr
            boost::shared_ptr<TMemoryBuffer> buf(
                new TMemoryBuffer(const_cast<uint8_t *>(p), static_cast<uint32_t>(n)));
            TBinaryProtocol proto(buf);
            dsn::rpc_address addr;
            addr.read(&proto);
            (void)addr.to_string();
            dsn::host_port hp;
            hp.read(&proto);
            (void)hp.to_string();
        } catch (const apache::thrift::TException &) {
        }
        return 0;
    }

    std::string s(reinterpret_cast<const char *>(p), n);
    dsn::host_port hp = dsn::host_port::from_string(s);
    (void)hp.to_string();
    dsn::rpc_address a1 = dsn::rpc_address::from_ip_port(s);
    (void)a1.to_string();
    if (a1) {
        dsn::host_port back = dsn::host_port::from_address(a1);
        (void)back.to_string();
    }
    if (mode & 2) {
        dsn::rpc_address a2 = dsn::rpc_address::from_host_port(s);
        (void)a2.to_string();
    }
    return 0;
}
