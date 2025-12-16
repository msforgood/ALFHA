#!/bin/bash
set -eux

ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/out_cov"
REPORT_DIR="$ROOT/reports"
PROFILE_DIR="$ROOT/profiles"
ZLIB_SRC="/home/minseo/alfha/targets/zlib/target"

FUZZER="${1:-compress_fuzzer}"
CORPUS_DIR="${2:-$ROOT/corpus/$FUZZER}"
RUN_SECONDS="${3:-600}"

mkdir -p "$REPORT_DIR/$FUZZER"
mkdir -p "$PROFILE_DIR/$FUZZER"
mkdir -p "$CORPUS_DIR"

ARTIFACT_DIR="$ROOT/artifacts/$FUZZER"
mkdir -p "$ARTIFACT_DIR"

PROFRAW="$PROFILE_DIR/$FUZZER/default.profraw"
PROFDATA="$PROFILE_DIR/$FUZZER/default.profdata"

# clang 메이저 버전 추출 (예: 18)
CLANG_MAJOR="$(clang --version | head -n1 | sed -n 's/.*clang version \([0-9]\+\).*/\1/p')"

LLVM_PROFDATA="llvm-profdata"
LLVM_COV="llvm-cov"

if [ -n "$CLANG_MAJOR" ] && command -v "llvm-profdata-$CLANG_MAJOR" >/dev/null 2>&1; then
  LLVM_PROFDATA="llvm-profdata-$CLANG_MAJOR"
fi
if [ -n "$CLANG_MAJOR" ] && command -v "llvm-cov-$CLANG_MAJOR" >/dev/null 2>&1; then
  LLVM_COV="llvm-cov-$CLANG_MAJOR"
fi


# 1) 퍼징 실행하면서 프로파일 수집
# -max_total_time는 초 단위
LLVM_PROFILE_FILE="$PROFRAW" \
  "$BIN/$FUZZER" "$CORPUS_DIR" -max_total_time="$RUN_SECONDS" -artifact_prefix="$ROOT/artifacts/$FUZZER/"

# 2) profraw -> profdata
"$LLVM_PROFDATA" merge -sparse "$PROFRAW" -o "$PROFDATA"

# 3) 텍스트 리포트(요약)
"$LLVM_COV" report "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
  > "$REPORT_DIR/$FUZZER/coverage_report.txt"

# 4) 상세 라인 커버리지(텍스트)
"$LLVM_COV" show "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -show-line-counts-or-regions \
  -show-branches=count \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
  > "$REPORT_DIR/$FUZZER/coverage_show.txt"

# 5) HTML 리포트
"$LLVM_COV" show "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -format=html \
  -output-dir="$REPORT_DIR/$FUZZER/html" \
  -Xdemangler=c++filt \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC"

echo "saved:"
echo "  $REPORT_DIR/$FUZZER/coverage_report.txt"
echo "  $REPORT_DIR/$FUZZER/coverage_show.txt"
echo "  $REPORT_DIR/$FUZZER/html/index.html"
