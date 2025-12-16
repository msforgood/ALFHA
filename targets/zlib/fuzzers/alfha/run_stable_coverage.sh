#!/bin/bash
set -e

# ALFHA zlib Stable Coverage Runner  
# Runs only verified stable harnesses in parallel

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$ROOT/out_cov"
PROFILE_DIR="$ROOT/stable_profiles"
CORPUS_BASE_DIR="$ROOT/corpus"

RUN_SECONDS_PER_HARNESS="${1:-30}"
PARALLEL_WORKERS="${2:-4}"

echo "🔍 ALFHA zlib Stable Coverage Runner"
echo "===================================="
echo "⏱️  Runtime per harness: ${RUN_SECONDS_PER_HARNESS}s"
echo "👥 Parallel workers: $PARALLEL_WORKERS"
echo ""

mkdir -p "$PROFILE_DIR"
mkdir -p "$CORPUS_BASE_DIR"

# Only stable harnesses (verified to work without major crashes)
STABLE_HARNESSES=(
    "deflateInit_harness"
    "deflate_harness"
    "inflate_harness" 
    "deflateCopy_harness"
    "deflateInit2_harness"
    "deflateSetDictionary_harness"
    "inflateSetDictionary_harness"
)

echo "📊 Running ${#STABLE_HARNESSES[@]} stable harnesses:"
for harness in "${STABLE_HARNESSES[@]}"; do
    echo "  ✓ $harness"
done
echo ""

# Function to run a single harness with better error handling
run_harness() {
    local harness="$1"
    local corpus_dir="$CORPUS_BASE_DIR/$harness"
    local profile_file="$PROFILE_DIR/${harness}.profraw"
    local artifact_dir="$ROOT/artifacts/$harness"
    
    echo "🚀 [$harness] Starting..."
    
    mkdir -p "$corpus_dir" "$artifact_dir"
    
    # Seed corpus
    if [ ! "$(ls -A "$corpus_dir" 2>/dev/null)" ]; then
        echo "test" > "$corpus_dir/seed1"
        echo -e "\x00\x01\x02\x03" > "$corpus_dir/seed2"
        printf '\x78\x9c\x63\x00\x00\x00\x01\x00\x01' > "$corpus_dir/seed3"
    fi
    
    # Set profile file
    export LLVM_PROFILE_FILE="$profile_file"
    
    # Run with memory limit and shorter timeout for stability
    timeout $((RUN_SECONDS_PER_HARNESS + 5)) \
        "$BIN_DIR/$harness" "$corpus_dir" \
        -max_total_time="$RUN_SECONDS_PER_HARNESS" \
        -artifact_prefix="$artifact_dir/" \
        -rss_limit_mb=512 \
        -print_final_stats=1 \
        > "$PROFILE_DIR/${harness}_output.log" 2>&1 || {
            local exit_code=$?
            echo "⚠️  [$harness] Exit code: $exit_code"
        }
    
    # Check result
    if [ -f "$profile_file" ] && [ -s "$profile_file" ]; then
        local size=$(stat -f%z "$profile_file" 2>/dev/null || stat -c%s "$profile_file" 2>/dev/null || echo "0")
        echo "✅ [$harness] Profile: ${size} bytes"
    else
        echo "❌ [$harness] No profile generated"
    fi
}

# Export function for parallel execution
export -f run_harness
export BIN_DIR CORPUS_BASE_DIR PROFILE_DIR ROOT RUN_SECONDS_PER_HARNESS

# Run harnesses in parallel using xargs
printf '%s\n' "${STABLE_HARNESSES[@]}" | \
    xargs -n 1 -P "$PARALLEL_WORKERS" -I {} bash -c 'run_harness "$@"' _ {}

echo ""
echo "📊 Results Summary:"
echo "  📁 Profiles generated:"
for profile in "$PROFILE_DIR"/*.profraw; do
    if [ -f "$profile" ] && [ -s "$profile" ]; then
        size=$(stat -f%z "$profile" 2>/dev/null || stat -c%s "$profile" 2>/dev/null || echo "0")
        name=$(basename "$profile" .profraw)
        printf "    %-25s %10s bytes\n" "$name" "$size"
    fi
done

echo ""
echo "🎉 Stable coverage collection completed!"
echo "🔗 Next: Run ./generate_stable_report.sh to create unified report"