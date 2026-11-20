#!/usr/bin/env bash
# mayhem/build.sh — build VTR's parser libraries (libarchfpga, libblifparse, libvtrutil, pugixml)
# sanitized with the fuzz harnesses, plus an unsanitized build of upstream's read_arch and
# blifparse_test tools for mayhem/test.sh. Uses mayhem/CMakeLists.txt (upstream's top-level CMake
# needs git submodules that the checkout does not carry). Build dirs live under mayhem/build/
# (gitignored by upstream's `build` pattern).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS STANDALONE_FUZZ_MAIN

cd "$SRC"

# libFuzzer coverage instrumentation for the library code too (not only the harness TU). Empty when
# LIB_FUZZING_ENGINE is empty.
COV=""
[ -n "$LIB_FUZZING_ENGINE" ] && COV="-fsanitize=fuzzer-no-link"

# 1+2) Sanitized libraries + harnesses (fuzzer and -standalone binaries). mayhem/lsan_off.cc (LeakSanitizer
#      off, SPEC 6.2 item 15) is linked into every fuzz and -standalone binary.
cmake -S mayhem -B mayhem/build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=None -DMAYHEM_FUZZ=ON \
      -DMAYHEM_LSAN_OFF="$SRC/mayhem/lsan_off.cc" \
      -DCMAKE_C_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS $COV" \
      -DCMAKE_CXX_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS $COV"
cmake --build mayhem/build/fuzz -j"$MAYHEM_JOBS"
for h in fuzz_read_arch fuzz_blifparse fuzz_expr_eval fuzz_rr_graph fuzz_server_telegram; do
  cp -f "mayhem/build/fuzz/$h" "mayhem/build/fuzz/$h-standalone" /mayhem/
done

# 3) Test tools with the project's normal flags (independent build).
cmake -S mayhem -B mayhem/build/test -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAYHEM_FUZZ=OFF \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build mayhem/build/test -j"$MAYHEM_JOBS" --target read_arch blifparse_test
