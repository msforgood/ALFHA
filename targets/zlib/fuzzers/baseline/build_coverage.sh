#!/bin/bash
set -eux

ROOT="$(cd "$(dirname "$0")" && pwd)"
HARNESS="$ROOT/harnesses/ossfuzz_zlib"
ZLIB_SRC="/home/minseo/alfha/targets/zlib/target"

mkdir -p "$ROOT/out_cov"

# zlib 빌드 (coverage 계측 포함)
cd "$ZLIB_SRC"

export CC=clang
export CXX=clang++
export CFLAGS="-O1 -g -fprofile-instr-generate -fcoverage-mapping -fno-omit-frame-pointer"
export CPPFLAGS=""
export LDFLAGS=""

./configure
make clean
make -j"$(nproc)"
cd "$ROOT"
# fuzzers 빌드 (libFuzzer + coverage 계측)
CC=clang
CXX=clang++
FUZZ_CFLAGS="-O1 -g -fsanitize=fuzzer,address -fno-omit-frame-pointer -fprofile-instr-generate -fcoverage-mapping"
FUZZ_CXXFLAGS="$FUZZ_CFLAGS"

$CC  $FUZZ_CFLAGS    "$HARNESS/compress_fuzzer.c" "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/compress_fuzzer"
$CC  $FUZZ_CFLAGS    "$HARNESS/checksum_fuzzer.c" "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/checksum_fuzzer"
# $CC  $FUZZ_CFLAGS    "$HARNESS/gzio_fuzzer.c"     "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/gzio_fuzzer" -> 에러로 스킵
$CC  $FUZZ_CFLAGS    "$HARNESS/minigzip_fuzzer.c" "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/minigzip_fuzzer"

$CXX $FUZZ_CXXFLAGS  "$HARNESS/zlib_uncompress_fuzzer.cc"  "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/zlib_uncompress_fuzzer"
$CXX $FUZZ_CXXFLAGS  "$HARNESS/zlib_uncompress2_fuzzer.cc" "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/zlib_uncompress2_fuzzer"
$CXX $FUZZ_CXXFLAGS  "$HARNESS/zlib_uncompress3_fuzzer.cc" "$ZLIB_SRC/libz.a" -o "$ROOT/out_cov/zlib_uncompress3_fuzzer"
