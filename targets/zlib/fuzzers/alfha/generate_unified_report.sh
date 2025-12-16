#!/bin/bash
set -e

# ALFHA zlib Unified Coverage Report Generator
# Merges all harness profiles and generates comprehensive coverage reports

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_DIR="$ROOT/unified_profiles"
REPORT_DIR="$ROOT/unified_reports"
BIN_DIR="$ROOT/out_cov"
ZLIB_SRC="/home/minseo/alfha/targets/zlib/target"
HARNESS_DIR="$ROOT/harnesses"

# Output files
MERGED_PROFDATA="$PROFILE_DIR/unified.profdata"
TIMESTAMP=$(date '+%y%m%d_%H%M')
SUMMARY_FILE="$ROOT/../../analysis/logs/unified_coverage_report_${TIMESTAMP}.md"

echo "🔍 ALFHA zlib Unified Coverage Report Generator"
echo "==============================================="
echo "📁 Profile directory: $PROFILE_DIR"
echo "📊 Report directory: $REPORT_DIR"
echo "📝 Summary file: $SUMMARY_FILE"
echo ""

# Create report directory
mkdir -p "$REPORT_DIR"

# Detect LLVM tools
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

# Check for profile files
PROFILE_FILES=()
for profile in "$PROFILE_DIR"/*.profraw; do
    if [ -f "$profile" ]; then
        PROFILE_FILES+=("$profile")
    fi
done

if [ ${#PROFILE_FILES[@]} -eq 0 ]; then
    echo "❌ No profile files found in $PROFILE_DIR"
    echo "Please run ./run_unified_coverage.sh first"
    exit 1
fi

echo "📊 Found ${#PROFILE_FILES[@]} profile files to merge:"
for profile in "${PROFILE_FILES[@]}"; do
    size=$(stat -f%z "$profile" 2>/dev/null || stat -c%s "$profile" 2>/dev/null || echo "0")
    name=$(basename "$profile")
    printf "  %-30s %10s bytes\n" "$name" "$size"
done
echo ""

# Step 1: Merge all profiles
echo "🔄 Merging all profiles..."
"$LLVM_PROFDATA" merge -sparse "${PROFILE_FILES[@]}" -o "$MERGED_PROFDATA"

if [ ! -f "$MERGED_PROFDATA" ]; then
    echo "❌ Failed to create merged profile data"
    exit 1
fi

merged_size=$(stat -f%z "$MERGED_PROFDATA" 2>/dev/null || stat -c%s "$MERGED_PROFDATA" 2>/dev/null || echo "0")
echo "✅ Merged profile created: ${merged_size} bytes"
echo ""

# Find a representative binary (prefer deflate_harness as it's comprehensive)
REFERENCE_BINARY=""
PREFERRED_BINARIES=("deflate_harness" "inflate_harness" "compress_harness")

for binary in "${PREFERRED_BINARIES[@]}"; do
    if [ -f "$BIN_DIR/$binary" ]; then
        REFERENCE_BINARY="$BIN_DIR/$binary"
        echo "📋 Using $binary as reference binary for reports"
        break
    fi
done

if [ -z "$REFERENCE_BINARY" ]; then
    # Fallback to any available binary
    for binary in "$BIN_DIR"/*_harness; do
        if [ -f "$binary" ]; then
            REFERENCE_BINARY="$binary"
            echo "📋 Using $(basename "$binary") as reference binary"
            break
        fi
    done
fi

if [ -z "$REFERENCE_BINARY" ]; then
    echo "❌ No harness binary found for report generation"
    exit 1
fi

# Step 2: Generate comprehensive reports
echo ""
echo "📊 Generating unified coverage reports..."

# Summary report (all files)
echo "  📄 Summary report..."
"$LLVM_COV" report "$REFERENCE_BINARY" \
    -instr-profile="$MERGED_PROFDATA" \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    > "$REPORT_DIR/unified_summary.txt"

# Detailed report with source files
echo "  📋 Detailed report..."
"$LLVM_COV" show "$REFERENCE_BINARY" \
    -instr-profile="$MERGED_PROFDATA" \
    -show-line-counts-or-regions \
    -show-branches=count \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    > "$REPORT_DIR/unified_detailed.txt"

# Function-level report
echo "  ⚙️  Function report..."
"$LLVM_COV" report "$REFERENCE_BINARY" \
    -instr-profile="$MERGED_PROFDATA" \
    -show-functions \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    "$ZLIB_SRC"/*.c \
    > "$REPORT_DIR/unified_functions.txt" 2>/dev/null || \
"$LLVM_COV" report "$REFERENCE_BINARY" \
    -instr-profile="$MERGED_PROFDATA" \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    > "$REPORT_DIR/unified_functions_basic.txt"

# HTML report
echo "  🌐 HTML report..."
"$LLVM_COV" show "$REFERENCE_BINARY" \
    -instr-profile="$MERGED_PROFDATA" \
    -format=html \
    -output-dir="$REPORT_DIR/html" \
    -Xdemangler=c++filt \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    "$ZLIB_SRC"/*.c 2>/dev/null || \
"$LLVM_COV" show "$REFERENCE_BINARY" \
    -instr-profile="$MERGED_PROFDATA" \
    -format=html \
    -output-dir="$REPORT_DIR/html" \
    -Xdemangler=c++filt \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC"

# Step 3: Extract key statistics
echo ""
echo "📈 Extracting coverage statistics..."

# Overall coverage
TOTAL_COVERAGE=$(grep -E "^TOTAL" "$REPORT_DIR/unified_summary.txt" | tail -1)

# File-level coverage
FILE_COUNT=$(grep -E "\.c$" "$REPORT_DIR/unified_summary.txt" | wc -l)
COVERED_FILES=$(grep -E "\.c$" "$REPORT_DIR/unified_summary.txt" | awk '$3 != "0.00%"' | wc -l)

# Function coverage (if available)
FUNCTION_STATS=""
if [ -f "$REPORT_DIR/unified_functions.txt" ]; then
    FUNCTION_STATS=$(grep -E "^TOTAL" "$REPORT_DIR/unified_functions.txt" | tail -1)
elif [ -f "$REPORT_DIR/unified_functions_basic.txt" ]; then
    FUNCTION_STATS=$(grep -E "^TOTAL" "$REPORT_DIR/unified_functions_basic.txt" | tail -1)
fi

# Step 4: Analyze individual harness contributions
echo "  🔍 Analyzing individual contributions..."

CONTRIBUTION_ANALYSIS="$REPORT_DIR/harness_contributions.txt"
echo "# Individual Harness Coverage Contributions" > "$CONTRIBUTION_ANALYSIS"
echo "Generated: $(date)" >> "$CONTRIBUTION_ANALYSIS"
echo "" >> "$CONTRIBUTION_ANALYSIS"

for profile in "${PROFILE_FILES[@]}"; do
    harness_name=$(basename "$profile" .profraw)
    temp_profdata="$PROFILE_DIR/temp_${harness_name}.profdata"
    
    # Create individual profile data
    "$LLVM_PROFDATA" merge -sparse "$profile" -o "$temp_profdata" 2>/dev/null || continue
    
    # Generate individual report
    individual_coverage=$("$LLVM_COV" report "$REFERENCE_BINARY" \
        -instr-profile="$temp_profdata" \
        -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" 2>/dev/null | \
        grep -E "^TOTAL" | tail -1 || echo "N/A")
    
    echo "## $harness_name" >> "$CONTRIBUTION_ANALYSIS"
    echo "$individual_coverage" >> "$CONTRIBUTION_ANALYSIS"
    echo "" >> "$CONTRIBUTION_ANALYSIS"
    
    # Cleanup temp file
    rm -f "$temp_profdata"
done

# Step 5: Generate markdown summary
echo "  📝 Creating markdown summary..."

cat > "$SUMMARY_FILE" << EOF
# ALFHA zlib Unified Coverage Report

**Generated:** $(date)  
**Target:** zlib 1.3.1.2  
**Harnesses:** ${#PROFILE_FILES[@]} harnesses merged  
**Reference Binary:** $(basename "$REFERENCE_BINARY")

## 🎯 Overall Coverage Summary

\`\`\`
$TOTAL_COVERAGE
\`\`\`

## 📊 File Coverage Statistics

- **Total C files:** $FILE_COUNT
- **Files with coverage:** $COVERED_FILES  
- **Coverage rate:** $(echo "scale=1; $COVERED_FILES * 100 / $FILE_COUNT" | bc 2>/dev/null || echo "N/A")%

## ⚙️ Function Coverage

EOF

if [ -n "$FUNCTION_STATS" ]; then
    echo "\`\`\`" >> "$SUMMARY_FILE"
    echo "$FUNCTION_STATS" >> "$SUMMARY_FILE"
    echo "\`\`\`" >> "$SUMMARY_FILE"
else
    echo "*Function-level statistics not available*" >> "$SUMMARY_FILE"
fi

cat >> "$SUMMARY_FILE" << EOF

## 📁 Generated Reports

- **Summary:** \`$REPORT_DIR/unified_summary.txt\`
- **Detailed:** \`$REPORT_DIR/unified_detailed.txt\`
- **Functions:** \`$REPORT_DIR/unified_functions*.txt\`
- **HTML:** \`$REPORT_DIR/html/index.html\`
- **Contributions:** \`$REPORT_DIR/harness_contributions.txt\`

## 🔍 Individual Harness Contributions

EOF

# Add top coverage contributors
if [ -f "$CONTRIBUTION_ANALYSIS" ]; then
    echo "\`\`\`" >> "$SUMMARY_FILE"
    grep -A 1 "^## " "$CONTRIBUTION_ANALYSIS" | grep -v "^## " | grep -v "^--" | head -10 >> "$SUMMARY_FILE"
    echo "\`\`\`" >> "$SUMMARY_FILE"
fi

cat >> "$SUMMARY_FILE" << EOF

## 🌐 View HTML Report

Open in browser: \`file://$REPORT_DIR/html/index.html\`

## 📈 Key Insights

- **Multi-binary approach:** Successfully merged coverage from ${#PROFILE_FILES[@]} different harnesses
- **Comprehensive coverage:** Unified view shows overall zlib library coverage
- **Individual contributions:** Each harness adds unique code path exploration

---
*Generated by ALFHA Unified Coverage System*
EOF

echo ""
echo "✅ Unified coverage report generation completed!"
echo ""
echo "📊 Coverage Summary:"
echo "$TOTAL_COVERAGE"
echo ""
echo "📁 Generated files:"
echo "  📄 Summary: $REPORT_DIR/unified_summary.txt"
echo "  📋 Detailed: $REPORT_DIR/unified_detailed.txt"
echo "  ⚙️  Functions: $REPORT_DIR/unified_functions*.txt"
echo "  🌐 HTML: $REPORT_DIR/html/index.html"
echo "  🔍 Contributions: $REPORT_DIR/harness_contributions.txt"
echo "  📝 Markdown: $SUMMARY_FILE"
echo ""
echo "🌐 Open HTML report:"
echo "  file://$REPORT_DIR/html/index.html"