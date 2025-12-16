#!/bin/bash
set -e

# ALFHA zlib Coverage Gap Analyzer
# Analyzes coverage gaps and harness contributions

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_DIR="$ROOT/unified_profiles"
REPORT_DIR="$ROOT/unified_reports"
ANALYSIS_DIR="$ROOT/coverage_analysis"
BIN_DIR="$ROOT/out_cov"
ZLIB_SRC="/home/minseo/alfha/targets/zlib/target"

echo "🔍 ALFHA zlib Coverage Gap Analyzer"
echo "===================================="
echo "📁 Analysis directory: $ANALYSIS_DIR"
echo ""

# Create analysis directory
mkdir -p "$ANALYSIS_DIR"

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

# Find reference binary
REFERENCE_BINARY=""
for binary in "$BIN_DIR"/deflate_harness "$BIN_DIR"/inflate_harness "$BIN_DIR"/*_harness; do
    if [ -f "$binary" ]; then
        REFERENCE_BINARY="$binary"
        break
    fi
done

if [ -z "$REFERENCE_BINARY" ]; then
    echo "❌ No harness binary found"
    exit 1
fi

echo "📋 Using $(basename "$REFERENCE_BINARY") as reference binary"
echo ""

# Check for unified profile
UNIFIED_PROFDATA="$PROFILE_DIR/unified.profdata"
if [ ! -f "$UNIFIED_PROFDATA" ]; then
    echo "❌ Unified profile not found: $UNIFIED_PROFDATA"
    echo "Please run ./generate_unified_report.sh first"
    exit 1
fi

# Step 1: Analyze individual harness coverage
echo "🔍 Analyzing individual harness contributions..."

INDIVIDUAL_ANALYSIS="$ANALYSIS_DIR/individual_analysis.txt"
HARNESS_SUMMARY="$ANALYSIS_DIR/harness_summary.csv"

echo "# Individual Harness Coverage Analysis" > "$INDIVIDUAL_ANALYSIS"
echo "Generated: $(date)" >> "$INDIVIDUAL_ANALYSIS"
echo "" >> "$INDIVIDUAL_ANALYSIS"

echo "harness,regions_total,regions_missed,region_coverage,functions_total,functions_missed,function_coverage,lines_total,lines_missed,line_coverage" > "$HARNESS_SUMMARY"

for profile in "$PROFILE_DIR"/*.profraw; do
    if [ ! -f "$profile" ]; then continue; fi
    
    harness_name=$(basename "$profile" .profraw)
    temp_profdata="$PROFILE_DIR/temp_${harness_name}.profdata"
    
    echo "  📊 Analyzing $harness_name..."
    
    # Create individual profile data
    "$LLVM_PROFDATA" merge -sparse "$profile" -o "$temp_profdata" 2>/dev/null || continue
    
    # Generate individual detailed report
    individual_report="$ANALYSIS_DIR/${harness_name}_individual.txt"
    "$LLVM_COV" report "$REFERENCE_BINARY" \
        -instr-profile="$temp_profdata" \
        -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
        > "$individual_report" 2>/dev/null
    
    # Extract coverage statistics
    total_line=$(grep -E "^TOTAL" "$individual_report" | tail -1)
    
    if [ -n "$total_line" ]; then
        # Parse the TOTAL line
        regions_total=$(echo "$total_line" | awk '{print $2}')
        regions_missed=$(echo "$total_line" | awk '{print $3}')  
        region_coverage=$(echo "$total_line" | awk '{print $4}')
        functions_total=$(echo "$total_line" | awk '{print $5}')
        functions_missed=$(echo "$total_line" | awk '{print $6}')
        function_coverage=$(echo "$total_line" | awk '{print $7}')
        lines_total=$(echo "$total_line" | awk '{print $8}')
        lines_missed=$(echo "$total_line" | awk '{print $9}')
        line_coverage=$(echo "$total_line" | awk '{print $10}')
        
        echo "$harness_name,$regions_total,$regions_missed,$region_coverage,$functions_total,$functions_missed,$function_coverage,$lines_total,$lines_missed,$line_coverage" >> "$HARNESS_SUMMARY"
        
        echo "## $harness_name" >> "$INDIVIDUAL_ANALYSIS"
        echo "$total_line" >> "$INDIVIDUAL_ANALYSIS"
        echo "" >> "$INDIVIDUAL_ANALYSIS"
    fi
    
    # Cleanup temp file
    rm -f "$temp_profdata"
done

# Step 2: Generate unified vs individual comparison
echo ""
echo "📈 Comparing unified vs individual coverage..."

COMPARISON_REPORT="$ANALYSIS_DIR/unified_vs_individual.txt"
echo "# Unified vs Individual Coverage Comparison" > "$COMPARISON_REPORT"
echo "Generated: $(date)" >> "$COMPARISON_REPORT"
echo "" >> "$COMPARISON_REPORT"

# Get unified coverage stats
if [ -f "$REPORT_DIR/unified_summary.txt" ]; then
    unified_total=$(grep -E "^TOTAL" "$REPORT_DIR/unified_summary.txt" | tail -1)
    echo "## Unified Coverage (All Harnesses)" >> "$COMPARISON_REPORT"
    echo "$unified_total" >> "$COMPARISON_REPORT"
    echo "" >> "$COMPARISON_REPORT"
fi

# Step 3: Identify coverage gaps
echo "  🔍 Identifying coverage gaps..."

GAP_ANALYSIS="$ANALYSIS_DIR/coverage_gaps.txt"
echo "# Coverage Gap Analysis" > "$GAP_ANALYSIS"
echo "Generated: $(date)" >> "$GAP_ANALYSIS"
echo "" >> "$GAP_ANALYSIS"

# Generate zero-coverage report (missed regions)
ZERO_COV_REPORT="$ANALYSIS_DIR/zero_coverage.txt"
"$LLVM_COV" show "$REFERENCE_BINARY" \
    -instr-profile="$UNIFIED_PROFDATA" \
    -show-line-counts-or-regions \
    -show-branches=count \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    > "$ZERO_COV_REPORT" 2>/dev/null

# Extract uncovered lines (lines with |     0|)
echo "## Uncovered Lines" >> "$GAP_ANALYSIS"
grep "|     0|" "$ZERO_COV_REPORT" | head -20 >> "$GAP_ANALYSIS" || echo "No uncovered lines found in sample" >> "$GAP_ANALYSIS"
echo "" >> "$GAP_ANALYSIS"

# Step 4: Function coverage analysis
echo "  ⚙️  Analyzing function coverage..."

FUNC_ANALYSIS="$ANALYSIS_DIR/function_analysis.txt"
echo "# Function Coverage Analysis" > "$FUNC_ANALYSIS"
echo "Generated: $(date)" >> "$FUNC_ANALYSIS"
echo "" >> "$FUNC_ANALYSIS"

# Generate function-level report if possible
FUNC_REPORT="$ANALYSIS_DIR/unified_functions_detailed.txt"
"$LLVM_COV" report "$REFERENCE_BINARY" \
    -instr-profile="$UNIFIED_PROFDATA" \
    -show-functions \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    "$ZLIB_SRC"/*.c \
    > "$FUNC_REPORT" 2>/dev/null || \
"$LLVM_COV" report "$REFERENCE_BINARY" \
    -instr-profile="$UNIFIED_PROFDATA" \
    -path-equivalence="$ZLIB_SRC","$ZLIB_SRC" \
    > "$FUNC_REPORT"

# Find functions with low coverage
echo "## Functions with Low Coverage (<50%)" >> "$FUNC_ANALYSIS"
if [ -f "$FUNC_REPORT" ]; then
    awk '$4 ~ /^[0-4][0-9]\./ || $4 ~ /^[0-9]\./' "$FUNC_REPORT" | head -20 >> "$FUNC_ANALYSIS" || echo "No low-coverage functions found" >> "$FUNC_ANALYSIS"
else
    echo "Function report not available" >> "$FUNC_ANALYSIS"
fi
echo "" >> "$FUNC_ANALYSIS"

echo "## Functions with Zero Coverage (0.00%)" >> "$FUNC_ANALYSIS"
if [ -f "$FUNC_REPORT" ]; then
    grep "0.00%" "$FUNC_REPORT" | head -20 >> "$FUNC_ANALYSIS" || echo "No zero-coverage functions found" >> "$FUNC_ANALYSIS"
else
    echo "Function report not available" >> "$FUNC_ANALYSIS"
fi

# Step 5: Generate recommendations
echo ""
echo "💡 Generating recommendations..."

RECOMMENDATIONS="$ANALYSIS_DIR/recommendations.txt"
echo "# Coverage Improvement Recommendations" > "$RECOMMENDATIONS"
echo "Generated: $(date)" >> "$RECOMMENDATIONS"
echo "" >> "$RECOMMENDATIONS"

# Count harnesses and coverage
harness_count=$(ls -1 "$PROFILE_DIR"/*.profraw 2>/dev/null | wc -l)
if [ -f "$REPORT_DIR/unified_summary.txt" ]; then
    overall_coverage=$(grep -E "^TOTAL" "$REPORT_DIR/unified_summary.txt" | awk '{print $4}' | tr -d '%' || echo "0")
else
    overall_coverage="0"
fi

cat >> "$RECOMMENDATIONS" << EOF
## Current Status
- **Active Harnesses:** $harness_count
- **Overall Coverage:** ${overall_coverage}%

## Recommendations for Coverage Improvement

### 🎯 High Priority
EOF

# Add specific recommendations based on coverage
if [ $(echo "$overall_coverage < 70" | bc 2>/dev/null || echo "1") -eq 1 ]; then
    cat >> "$RECOMMENDATIONS" << EOF
1. **Increase fuzzing duration** - Current coverage suggests more time needed
2. **Add error path testing** - Focus on error handling and edge cases  
3. **Expand input diversity** - Use more varied corpus seeds
EOF
fi

cat >> "$RECOMMENDATIONS" << EOF

### 📊 Medium Priority
1. **Analyze zero-coverage functions** - Target specific uncovered functions
2. **Cross-harness corpus sharing** - Use successful inputs across harnesses
3. **Parameter space expansion** - Test boundary values more thoroughly

### 🔧 Low Priority  
1. **Performance optimization** - Improve fuzzer execution speed
2. **Corpus minimization** - Reduce redundant test cases
3. **Custom mutators** - Implement domain-specific input generation

## 📈 Coverage Gap Analysis Summary

See detailed analysis in:
- Individual analysis: $INDIVIDUAL_ANALYSIS
- Coverage gaps: $GAP_ANALYSIS  
- Function analysis: $FUNC_ANALYSIS
EOF

# Step 6: Create summary dashboard
TIMESTAMP=$(date '+%y%m%d_%H%M')
DASHBOARD="$ROOT/../../analysis/logs/coverage_analysis_${TIMESTAMP}.md"

echo "  📊 Creating analysis dashboard..."

cat > "$DASHBOARD" << EOF
# ALFHA zlib Coverage Analysis Dashboard

**Generated:** $(date)  
**Analysis Type:** Gap Analysis & Optimization Recommendations  
**Harnesses Analyzed:** $harness_count

## 📊 Quick Stats

EOF

if [ -f "$HARNESS_SUMMARY" ]; then
    echo "### Top Performing Harnesses (by Line Coverage)" >> "$DASHBOARD"
    echo "" >> "$DASHBOARD"
    echo "| Harness | Line Coverage | Function Coverage | Regions Coverage |" >> "$DASHBOARD"
    echo "|---------|---------------|-------------------|------------------|" >> "$DASHBOARD"
    
    # Sort by line coverage and show top 5
    tail -n +2 "$HARNESS_SUMMARY" | sort -t, -k9 -nr | head -5 | while IFS=, read harness regions_total regions_missed region_coverage functions_total functions_missed function_coverage lines_total lines_missed line_coverage; do
        echo "| $harness | $line_coverage | $function_coverage | $region_coverage |" >> "$DASHBOARD"
    done
fi

cat >> "$DASHBOARD" << EOF

## 📁 Detailed Analysis Files

- **Individual Analysis:** \`$INDIVIDUAL_ANALYSIS\`
- **Coverage Gaps:** \`$GAP_ANALYSIS\`
- **Function Analysis:** \`$FUNC_ANALYSIS\`
- **Recommendations:** \`$RECOMMENDATIONS\`
- **Raw Data:** \`$HARNESS_SUMMARY\`

## 🎯 Key Insights

1. **Coverage Distribution:** Analysis of how different harnesses contribute
2. **Gap Identification:** Specific lines and functions not covered
3. **Optimization Targets:** Recommendations for improving coverage

## 🔧 Usage

Use this analysis to:
- Prioritize harness improvements
- Focus fuzzing efforts on gaps
- Understand coverage overlap between harnesses

---
*Generated by ALFHA Coverage Gap Analyzer*
EOF

echo ""
echo "✅ Coverage gap analysis completed!"
echo ""
echo "📁 Generated analysis files:"
echo "  📊 Individual analysis: $INDIVIDUAL_ANALYSIS"
echo "  🔍 Coverage gaps: $GAP_ANALYSIS"
echo "  ⚙️  Function analysis: $FUNC_ANALYSIS"
echo "  💡 Recommendations: $RECOMMENDATIONS"
echo "  📈 Harness summary: $HARNESS_SUMMARY"
echo "  📊 Dashboard: $DASHBOARD"
echo ""
echo "📋 Next steps:"
echo "  1. Review recommendations in: $RECOMMENDATIONS"
echo "  2. Check coverage gaps in: $GAP_ANALYSIS"
echo "  3. View dashboard: $DASHBOARD"