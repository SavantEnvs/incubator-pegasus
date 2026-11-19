#!/usr/bin/env bash
# mayhem/test.sh — runs upstream's own gtest unit tests for the code the harnesses fuzz
# (src/utils/test/{string_conv_test,time_utils_test,configuration}.cpp), built by build.sh
# with normal flags. The configuration tests load the config-*.ini fixtures from their
# directory, so the runner executes there.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER="$SRC/build/mayhem/test/dsn_utils_mayhem_test"
if [ ! -x "$RUNNER" ]; then
  echo "test.sh: $RUNNER missing — run mayhem/build.sh first" >&2
  emit_ctrf gtest 0 1
  exit 1
fi

XML="$(mktemp "${TMPDIR:-/tmp}/gtest-XXXXXX.xml")"
rm -f "$XML"
(cd "$SRC/src/utils/test" && "$RUNNER" --gtest_output="xml:$XML")
rc=$?

if [ ! -s "$XML" ]; then
  echo "test.sh: runner produced no report (rc=$rc)" >&2
  emit_ctrf gtest 0 1
  exit 1
fi
attr() { grep -m1 -o "<testsuites [^>]*" "$XML" | grep -o " $1=\"[0-9]*\"" | grep -o '[0-9]*'; }
total=$(attr tests); failures=$(attr failures); errors=$(attr errors); skipped=$(attr skipped)
: "${skipped:=0}" "${errors:=0}"
failed=$(( failures + errors ))
passed=$(( total - failed - skipped ))
rm -f "$XML"
[ "$rc" -eq 0 ] || [ "$failed" -gt 0 ] || failed=1
emit_ctrf gtest "$passed" "$failed" "$skipped"
