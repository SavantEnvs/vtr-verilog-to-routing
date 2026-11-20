#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
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

# Known-answer / golden-output tests over upstream's own tools, built by build.sh with normal flags:
#  * read_arch (libs/libarchfpga/src/main.cpp) parses an architecture XML and dumps the parsed data
#    structures (EchoArch); the dump must match the committed golden byte for byte.
#  * blifparse_test (libs/EXTERNAL/libblifparse/src/main.cpp) parses a BLIF/EBLIF file and pretty-prints
#    it back; the output must match the committed golden.
# Goldens live in mayhem/test/golden/ and were produced from this upstream revision.
B="$SRC/mayhem/build/test"
G="$SRC/mayhem/test/golden"
for t in "$B/read_arch" "$B/blifparse_test"; do
  [ -x "$t" ] || { echo "missing $t — run mayhem/build.sh first" >&2; emit_ctrf golden-diff 0 1; exit 1; }
done
work=$(mktemp -d "${TMPDIR:-/tmp}/vtr-test.XXXXXX")
pass=0; fail=0
check() {  # check <name> <produced> <golden>
  if [ -s "$2" ] && cmp -s "$2" "$3"; then pass=$((pass+1)); echo "PASS $1"
  else fail=$((fail+1)); echo "FAIL $1"; diff "$3" "$2" | head -20; fi
}
for a in k4_N4_90nm k6_N10_40nm k6_frac_N10_frac_chain_mem32K_40nm EArch; do
  rm -f "$work/$a.echo"
  "$B/read_arch" "vtr_flow/arch/timing/$a.xml" 1 "$work/$a.echo" > "$work/$a.log" 2>&1 || true
  check "read_arch:$a" "$work/$a.echo" "$G/arch_$a.echo"
done
BLIFS="vtr_flow/benchmarks/microbenchmarks/and.blif vtr_flow/benchmarks/microbenchmarks/and2_latch.blif
vtr_flow/benchmarks/microbenchmarks/mult_4x4.blif vtr_flow/benchmarks/microbenchmarks/adder_chains.blif
vtr_flow/benchmarks/microbenchmarks/and_latch.blif libs/EXTERNAL/libblifparse/test/eblif/test_blank_param_attr.eblif"
for f in $BLIFS; do
  n=$(basename "$f")
  "$B/blifparse_test" "$f" > "$work/$n.out" 2>&1 || true
  check "blifparse:$n" "$work/$n.out" "$G/blif_$n.out"
done
rm -rf "$work"
emit_ctrf golden-diff "$pass" "$fail"
