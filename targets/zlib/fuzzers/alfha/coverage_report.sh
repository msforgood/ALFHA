#!/bin/bash
set -eux

# ALFHA zlib Coverage Report Script
# Uses llvm-cov to generate comprehensive coverage reports

ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/out_cov"
REPORT_DIR="$ROOT/reports"
PROFILE_DIR="$ROOT/profiles"
ZLIB_SRC="/home/minseo/alfha/targets/zlib/target"

FUZZER="${1:-compress_harness}"
CORPUS_DIR="${2:-$ROOT/corpus/$FUZZER}"
RUN_SECONDS="${3:-10}"

echo "🔍 ALFHA zlib Coverage Report"
echo "============================="
echo "📊 Fuzzer: $FUZZER"
echo "📁 Corpus: $CORPUS_DIR"
echo "⏱️  Runtime: ${RUN_SECONDS}s"
echo ""

mkdir -p "$REPORT_DIR/$FUZZER"
mkdir -p "$PROFILE_DIR/$FUZZER"
mkdir -p "$CORPUS_DIR"

ARTIFACT_DIR="$ROOT/artifacts/$FUZZER"
mkdir -p "$ARTIFACT_DIR"

PROFRAW="$PROFILE_DIR/$FUZZER/default.profraw"
PROFDATA="$PROFILE_DIR/$FUZZER/default.profdata"

# Detect clang version for proper llvm tools
CLANG_MAJOR="$(clang --version | head -n1 | sed -n 's/.*clang version \([0-9]\+\).*/\1/p')"

LLVM_PROFDATA="llvm-profdata"
LLVM_COV="llvm-cov"

if [ -n "$CLANG_MAJOR" ] && command -v "llvm-profdata-$CLANG_MAJOR" >/dev/null 2>&1; then
  LLVM_PROFDATA="llvm-profdata-$CLANG_MAJOR"
fi
if [ -n "$CLANG_MAJOR" ] && command -v "llvm-cov-$CLANG_MAJOR" >/dev/null 2>&1; then
  LLVM_COV="llvm-cov-$CLANG_MAJOR"
fi

echo "🔧 Using LLVM tools:"
echo "   profdata: $LLVM_PROFDATA"
echo "   cov: $LLVM_COV"
echo ""

# Check if coverage build exists
if [ ! -f "$BIN/$FUZZER" ]; then
    echo "❌ Coverage build not found: $BIN/$FUZZER"
    echo "Please run: ./build_coverage.sh first"
    exit 1
fi

# Seed corpus with some basic test cases if empty
if [ ! "$(ls -A "$CORPUS_DIR" 2>/dev/null)" ]; then
    echo "📝 Seeding empty corpus with test cases..."
    echo "test" > "$CORPUS_DIR/test1"
    echo -e "\x00\x00\x00\x00" > "$CORPUS_DIR/test2"
    printf '\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00' > "$CORPUS_DIR/test3"
fi

echo "🚀 Running fuzzer with profile collection..."
# Run fuzzer with profile collection
LLVM_PROFILE_FILE="$PROFRAW" \
  timeout $((RUN_SECONDS + 5)) \
  "$BIN/$FUZZER" "$CORPUS_DIR" -max_total_time="$RUN_SECONDS" -artifact_prefix="$ARTIFACT_DIR/" \
  || echo "Fuzzer completed (timeout or normal exit)"

# Check if profile data was generated
if [ ! -f "$PROFRAW" ]; then
    echo "❌ Profile data not generated: $PROFRAW"
    exit 1
fi

echo "📊 Processing coverage data..."
# Convert profraw to profdata
"$LLVM_PROFDATA" merge -sparse "$PROFRAW" -o "$PROFDATA"

# Generate text summary report
echo "📝 Generating summary report..."
"$LLVM_COV" report "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
  > "$REPORT_DIR/$FUZZER/coverage_report.txt"

# Generate detailed line coverage
echo "📋 Generating detailed coverage..."
"$LLVM_COV" show "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -show-line-counts-or-regions \
  -show-branches=count \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
  > "$REPORT_DIR/$FUZZER/coverage_show.txt"

# Generate HTML report
echo "🌐 Generating HTML report..."
"$LLVM_COV" show "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -format=html \
  -output-dir="$REPORT_DIR/$FUZZER/html" \
  -Xdemangler=c++filt \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC"

# Generate function coverage summary with source files
echo "⚙️  Generating function coverage..."
"$LLVM_COV" report "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -show-functions \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
  "$ZLIB_SRC"/*.c "$HARNESS_DIR/${FUZZER}.c" \
  > "$REPORT_DIR/$FUZZER/function_coverage.txt" 2>/dev/null || \
"$LLVM_COV" report "$BIN/$FUZZER" \
  -instr-profile="$PROFDATA" \
  -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
  > "$REPORT_DIR/$FUZZER/function_coverage.txt"

# Extract coverage statistics
COVERAGE_STATS=$(grep -E "^TOTAL" "$REPORT_DIR/$FUZZER/coverage_report.txt" | tail -1)

# Create markdown summary
TIMESTAMP=$(date '+%y%m%d_%H%M')
SUMMARY_FILE="$ROOT/../../analysis/logs/coverage_report_${TIMESTAMP}.md"

cat > "$SUMMARY_FILE" << EOF
# ALFHA zlib Coverage Report

**Generated:** $(date)  
**Target:** zlib 1.3.1.2  
**Fuzzer:** $FUZZER  
**Runtime:** ${RUN_SECONDS}s

## Coverage Summary

\`\`\`
$COVERAGE_STATS
\`\`\`

## Top Functions by Coverage

\`\`\`
$(head -20 "$REPORT_DIR/$FUZZER/function_coverage.txt")
\`\`\`

## Files Generated

- **Summary:** \`$REPORT_DIR/$FUZZER/coverage_report.txt\`
- **Detailed:** \`$REPORT_DIR/$FUZZER/coverage_show.txt\`  
- **Functions:** \`$REPORT_DIR/$FUZZER/function_coverage.txt\`
- **HTML:** \`$REPORT_DIR/$FUZZER/html/index.html\`

## Artifacts

- **Corpus:** \`$CORPUS_DIR\`
- **Crashes:** \`$ARTIFACT_DIR\`

EOF

echo ""
echo "✅ Coverage report generated successfully!"
echo ""
echo "📊 Coverage Summary:"
echo "$COVERAGE_STATS"
echo ""
echo "📁 Generated files:"
echo "  📄 Summary: $REPORT_DIR/$FUZZER/coverage_report.txt"
echo "  📋 Detailed: $REPORT_DIR/$FUZZER/coverage_show.txt"
echo "  ⚙️  Functions: $REPORT_DIR/$FUZZER/function_coverage.txt"
echo "  🌐 HTML: $REPORT_DIR/$FUZZER/html/index.html"
echo "  📝 Markdown: $SUMMARY_FILE"
echo ""
echo "🌐 Open HTML report:"
echo "  file://$REPORT_DIR/$FUZZER/html/index.html"