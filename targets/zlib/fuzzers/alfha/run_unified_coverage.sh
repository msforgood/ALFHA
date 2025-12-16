#!/bin/bash
set -e

# ALFHA zlib Unified Coverage Runner
# Executes all harnesses and merges their coverage profiles

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$ROOT/out_cov"
PROFILE_DIR="$ROOT/unified_profiles"
CORPUS_BASE_DIR="$ROOT/corpus"
ZLIB_SRC="/home/minseo/alfha/targets/zlib/target"

# Default parameters
RUN_SECONDS_PER_HARNESS="${1:-30}"
PARALLEL_WORKERS="${2:-1}"
SKIP_EXISTING="${3:-false}"

echo "🔍 ALFHA zlib Unified Coverage Runner"
echo "====================================="
echo "⏱️  Runtime per harness: ${RUN_SECONDS_PER_HARNESS}s"
echo "👥 Parallel workers: $PARALLEL_WORKERS"
echo "📁 Profile directory: $PROFILE_DIR"
echo ""

# Create directories
mkdir -p "$PROFILE_DIR"
mkdir -p "$CORPUS_BASE_DIR"

# All available harnesses
HARNESSES=(
    "deflateInit_harness"
    "deflate_harness" 
    "deflateEnd_harness"
    "inflateInit_harness"
    "inflate_harness"
    "inflateEnd_harness"
    "compress_harness"
    "uncompress_harness"
    "compress2_harness"
    "uncompress2_harness"
    "inflateInit2_harness"
    "inflateSetDictionary_harness"
    "deflateCopy_harness"
    "deflateInit2_harness"
    "deflateSetDictionary_harness"
)

# Check which harnesses are available
AVAILABLE_HARNESSES=()
for harness in "${HARNESSES[@]}"; do
    if [ -f "$BIN_DIR/$harness" ]; then
        AVAILABLE_HARNESSES+=("$harness")
    else
        echo "⚠️  Skipping $harness (binary not found)"
    fi
done

echo "📊 Found ${#AVAILABLE_HARNESSES[@]} harnesses to run:"
for harness in "${AVAILABLE_HARNESSES[@]}"; do
    echo "  ✓ $harness"
done
echo ""

# Function to run a single harness
run_harness() {
    local harness="$1"
    local harness_name=$(basename "$harness" "_harness")
    local corpus_dir="$CORPUS_BASE_DIR/$harness"
    local profile_file="$PROFILE_DIR/${harness}.profraw"
    local artifact_dir="$ROOT/artifacts/$harness"
    
    # Skip if profile exists and SKIP_EXISTING is true
    if [ "$SKIP_EXISTING" = "true" ] && [ -f "$profile_file" ]; then
        echo "⏭️  Skipping $harness (profile exists)"
        return 0
    fi
    
    echo "🚀 Running $harness for ${RUN_SECONDS_PER_HARNESS}s..."
    
    # Create corpus directory and seed if empty
    mkdir -p "$corpus_dir"
    mkdir -p "$artifact_dir"
    
    if [ ! "$(ls -A "$corpus_dir" 2>/dev/null)" ]; then
        echo "📝 Seeding corpus for $harness..."
        echo "test" > "$corpus_dir/seed1"
        echo -e "\x00\x00\x00\x00" > "$corpus_dir/seed2"
        printf '\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00' > "$corpus_dir/seed3"
        
        # Add function-specific seeds
        case "$harness_name" in
            "deflateInit"|"inflateInit"|"deflateInit2"|"inflateInit2")
                # Initialization functions - add parameter-like data
                printf '\x01\x02\x03\x04\x05\x06\x07\x08' > "$corpus_dir/params1"
                printf '\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8' > "$corpus_dir/params2"
                ;;
            "deflate"|"inflate")
                # Core compression/decompression - add various data patterns
                python3 -c "import random; print(''.join(chr(random.randint(0,255)) for _ in range(100)))" > "$corpus_dir/random_data" 2>/dev/null || true
                yes "A" | head -c 100 > "$corpus_dir/repeated_data" 2>/dev/null || true
                ;;
            "compress"|"compress2"|"uncompress"|"uncompress2")
                # Buffer functions - add size-varied data
                dd if=/dev/zero bs=1 count=50 of="$corpus_dir/zeros" 2>/dev/null || true
                dd if=/dev/urandom bs=1 count=200 of="$corpus_dir/urandom" 2>/dev/null || true
                ;;
        esac
    fi
    
    # Set profile file for this harness
    export LLVM_PROFILE_FILE="$profile_file"
    
    # Run harness with timeout and capture output
    local start_time=$(date +%s)
    timeout $((RUN_SECONDS_PER_HARNESS + 10)) \
        "$BIN_DIR/$harness" "$corpus_dir" \
        -max_total_time="$RUN_SECONDS_PER_HARNESS" \
        -artifact_prefix="$artifact_dir/" \
        -print_final_stats=1 \
        > "$PROFILE_DIR/${harness}_output.log" 2>&1 || {
            local exit_code=$?
            if [ $exit_code -eq 124 ]; then
                echo "⏰ $harness completed (timeout)"
            else
                echo "⚠️  $harness exited with code $exit_code"
            fi
        }
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Check if profile was generated
    if [ -f "$profile_file" ]; then
        local size=$(stat -f%z "$profile_file" 2>/dev/null || stat -c%s "$profile_file" 2>/dev/null || echo "0")
        echo "✅ $harness completed in ${duration}s (profile: ${size} bytes)"
        
        # Extract basic stats from log
        if [ -f "$PROFILE_DIR/${harness}_output.log" ]; then
            local executions=$(grep "stat::number_of_executed_units:" "$PROFILE_DIR/${harness}_output.log" | tail -1 | cut -d: -f2 | tr -d ' ' || echo "0")
            local new_units=$(grep "stat::new_units_added:" "$PROFILE_DIR/${harness}_output.log" | tail -1 | cut -d: -f2 | tr -d ' ' || echo "0")
            echo "  📊 Executions: $executions, New units: $new_units"
        fi
    else
        echo "❌ $harness failed to generate profile"
        return 1
    fi
}

# Function to run harnesses in parallel
run_parallel() {
    local max_jobs="$1"
    shift
    local harnesses=("$@")
    
    for harness in "${harnesses[@]}"; do
        # Wait if we have too many background jobs
        while [ $(jobs -r | wc -l) -ge "$max_jobs" ]; do
            sleep 1
        done
        
        # Run harness in background
        run_harness "$harness" &
    done
    
    # Wait for all background jobs to complete
    wait
}

# Run all harnesses
echo "🎯 Starting harness execution..."
start_total=$(date +%s)

if [ "$PARALLEL_WORKERS" -gt 1 ]; then
    echo "🔄 Running with $PARALLEL_WORKERS parallel workers..."
    run_parallel "$PARALLEL_WORKERS" "${AVAILABLE_HARNESSES[@]}"
else
    echo "🔄 Running sequentially..."
    for harness in "${AVAILABLE_HARNESSES[@]}"; do
        run_harness "$harness"
    done
fi

end_total=$(date +%s)
duration_total=$((end_total - start_total))

echo ""
echo "📊 Execution Summary:"
echo "  ⏱️  Total time: ${duration_total}s"
echo "  📁 Profiles generated: $(ls -1 "$PROFILE_DIR"/*.profraw 2>/dev/null | wc -l)"
echo "  📋 Logs available: $(ls -1 "$PROFILE_DIR"/*_output.log 2>/dev/null | wc -l)"
echo ""

# List generated profiles with sizes
echo "📄 Generated profiles:"
for profile in "$PROFILE_DIR"/*.profraw 2>/dev/null; do
    if [ -f "$profile" ]; then
        local size=$(stat -f%z "$profile" 2>/dev/null || stat -c%s "$profile" 2>/dev/null || echo "0")
        local name=$(basename "$profile" .profraw)
        printf "  %-25s %10s bytes\n" "$name" "$size"
    fi
done

echo ""
echo "🎉 Unified coverage data collection completed!"
echo "🔗 Next step: Run ./generate_unified_report.sh to create merged coverage report"