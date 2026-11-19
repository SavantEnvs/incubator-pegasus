#!/usr/bin/env bash
# mayhem/build.sh — builds Pegasus' rDSN utility library (src/utils) with the fuzz flags,
# the libFuzzer harnesses in mayhem/harnesses, their -standalone reproducers, and a
# normal-flags gtest runner over upstream's own src/utils unit tests (for test.sh).
#
# Upstream's CMake build needs ~25 third-party projects built from source (thirdparty/,
# ExternalProject) and the full rDSN runtime; the parsers fuzzed here only need fmt,
# abseil, rocksdb headers, boost, openssl and the thrift-generated utils/dsn types, all of
# which come from pinned Debian packages, so the relevant upstream sources are compiled
# directly. Re-runnable and offline: nothing is fetched.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
cd "$SRC"

B="$SRC/build/mayhem"          # build/ is gitignored upstream
GEN="$B/thrift-gen"
mkdir -p "$GEN" "$B/fuzz" "$B/test"

# 1) thrift-generated types the utils headers include (utils_types.h etc.).
THRIFT_HOME=/opt/toolchains/deps/thrift-0.9.3   # upstream's pinned thrift (Dockerfile)
for t in idl/utils.thrift idl/metadata.thrift idl/dsn.layer2.thrift idl/meta_admin.thrift \
         idl/duplication.thrift idl/backup.thrift idl/bulk_load.thrift idl/partition_split.thrift \
         idl/replica_admin.thrift src/common/consensus.thrift src/common/duplication_internal.thrift; do
    "$THRIFT_HOME/bin/thrift" -gen cpp:moveable_types --out "$GEN" "$t"
done
rm -f "$GEN"/*skeleton*
# Same include-guard fix upstream's build_tools/compile_thrift.py applies.
sed -i 's/dsn\.layer2_TYPES_H/dsn_layer2_TYPES_H/' "$GEN/dsn.layer2_types.h"
mkdir -p "$GEN/common/serialization_helper" && cp "$GEN/dsn.layer2_types.h" "$GEN/common/serialization_helper/"

# Upstream src/utils translation units that do not need the rDSN runtime / RPC layer.
UTILS_SRCS=(
    utils/configuration utils/strings utils/env utils/crc utils/filesystem utils/error_code
    utils/rand utils/clock utils/time_utils utils/throttling_controller utils/token_bucket_throttling_controller
    utils/process_utils utils/safe_strerror_posix utils/thread_access_checker utils/long_adder
    utils/math utils/fail_point utils/gpid utils/alloc utils/config_api utils/command_manager
    utils/flags utils/output_utils task/task_code task/task_spec runtime/threadpool_code
    rpc/rpc_address rpc/rpc_host_port
    GEN:dsn.layer2_types GEN:metadata_types GEN:bulk_load_types GEN:utils_types
)
COMMON_INC=(-I"$SRC/src" -I"$GEN" -I"$SRC/src/common/serialization_helper" -I"$THRIFT_HOME/include")
SYSLIBS=("$THRIFT_HOME/lib/libthrift.a" -lrocksdb -lfmt -lboost_system -lboost_filesystem -lcrypto -lssl -labsl_strings -labsl_str_format_internal -lpthread -ldl -lm)

compile_lib() { # $1 = outdir, $2.. = flags
    local out="$1"; shift
    local objs=() pids=() f o
    for f in "${UTILS_SRCS[@]}"; do
        o="$out/${f//[\/:]/_}.o"
        local src="$SRC/src/$f.cpp"
        [ "${f#GEN:}" = "$f" ] || src="$GEN/${f#GEN:}.cpp"
        objs+=("$o")
        # always recompile (headers are not tracked here; ccache makes this cheap)
        "$CXX" -std=c++17 -w "$@" -D__FILENAME__="\"${f##*[/:]}.cpp\"" "${COMMON_INC[@]}" \
            -c "$src" -o "$o" &
        pids+=($!)
        if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}" || exit 1; pids=("${pids[@]:1}"); fi
    done
    for p in "${pids[@]}"; do wait "$p" || exit 1; done
    rm -f "$out/libdsn_utils_fuzz.a"
    ar rcs "$out/libdsn_utils_fuzz.a" "${objs[@]}"
}

# 2) Fuzz build: project + harnesses with sanitizers and DWARF 3.
# -ffunction-sections + --gc-sections: rpc_host_port.cpp's host_port::resolve() needs the
# runtime's dns_resolver/metrics, which no harness calls; the linker drops it.
FFLAGS=(-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -ffunction-sections -fdata-sections)
compile_lib "$B/fuzz" "${FFLAGS[@]}"
"$CXX" -std=c++17 -w "${FFLAGS[@]}" "${COMMON_INC[@]}" -D__FILENAME__='"runtime_shim.cc"' \
    -c mayhem/harnesses/runtime_shim.cc -o "$B/fuzz/runtime_shim.o"
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/fuzz/lsan_off.o"
"$CC" $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o"

for h in fuzz_configuration fuzz_strings fuzz_throttling_env fuzz_host_port fuzz_json_metadata; do
    extra=()
    # rapidjson 1.1.0 (Debian, = upstream's pin) does `stackTop_ += n` on a null stack in
    # internal/stack.h:117 for every document: UBSan pointer-overflow fires on every input.
    # Relax only that check, only in the TU that instantiates rapidjson's reader/writer.
    [ "$h" = fuzz_json_metadata ] && [ -n "$SANITIZER_FLAGS" ] && extra=(-fno-sanitize=pointer-overflow)
    "$CXX" -std=c++17 -w "${FFLAGS[@]}" "${extra[@]}" "${COMMON_INC[@]}" -D__FILENAME__="\"$h.cc\"" \
        -c "mayhem/harnesses/$h.cc" -o "$B/fuzz/$h.o"
    LINK=("$B/fuzz/$h.o" "$B/fuzz/runtime_shim.o" "$B/fuzz/lsan_off.o" "$B/fuzz/libdsn_utils_fuzz.a" "${SYSLIBS[@]}" -Wl,--gc-sections)
    "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "${LINK[@]}" -o "$SRC/$h"
    "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$B/fuzz/standalone_main.o" "${LINK[@]}" -o "$SRC/$h-standalone"
done

# 3) Test build: upstream's own unit tests for these modules, NORMAL flags, no sanitizers.
TFLAGS=(-O1 $COVERAGE_FLAGS -ffunction-sections -fdata-sections)
compile_lib "$B/test" "${TFLAGS[@]}"
"$CXX" -std=c++17 -w "${TFLAGS[@]}" "${COMMON_INC[@]}" -D__FILENAME__='"runtime_shim.cc"' \
    -c mayhem/harnesses/runtime_shim.cc -o "$B/test/runtime_shim.o"
TESTS=(string_conv_test time_utils_test configuration)
tobjs=()
for t in "${TESTS[@]}"; do
    "$CXX" -std=c++17 -w "${TFLAGS[@]}" "${COMMON_INC[@]}" -D__FILENAME__="\"$t.cpp\"" \
        -c "src/utils/test/$t.cpp" -o "$B/test/$t.o"
    tobjs+=("$B/test/$t.o")
done
"$CXX" $COVERAGE_FLAGS "${tobjs[@]}" "$B/test/runtime_shim.o" "$B/test/libdsn_utils_fuzz.a" \
    -lgtest_main -lgtest "${SYSLIBS[@]}" -o "$B/test/dsn_utils_mayhem_test"
echo "build.sh: done"
