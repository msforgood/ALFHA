#!/bin/bash

# ALFHA zlib Fuzzer Run Script
# Executes fuzzing harnesses with proper crash artifact management

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_DIR="$SCRIPT_DIR/harnesses"
CORPUS_DIR="$SCRIPT_DIR/corpus"
ARTIFACTS_DIR="$SCRIPT_DIR/artifacts"

# Default values
DURATION=60
HARNESS=""
MODE="single"
WORKERS=1

usage() {
    echo "🚀 ALFHA zlib Fuzzer Run Script"
    echo "==============================="
    echo ""
    echo "Usage: $0 [OPTIONS] [HARNESS_NAME]"
    echo ""
    echo "Options:"
    echo "  -t, --time SECONDS     Fuzzing duration (default: 60)"
    echo "  -w, --workers COUNT    Number of parallel workers (default: 1)"
    echo "  -a, --all             Run all harnesses sequentially"
    echo "  -p, --parallel        Run all harnesses in parallel"
    echo "  -l, --list            List available harnesses"
    echo "  -h, --help            Show this help"
    echo ""
    echo "Available harnesses:"
    echo "  deflateInit_harness    - Test deflate initialization"
    echo "  deflate_harness        - Test deflate compression"  
    echo "  deflateEnd_harness     - Test deflate cleanup"
    echo "  inflateInit_harness    - Test inflate initialization"
    echo "  inflate_harness        - Test inflate decompression"
    echo "  inflateEnd_harness     - Test inflate cleanup"
    echo "  compress_harness       - Test simple compression"
    echo "  uncompress_harness     - Test simple decompression"
    echo ""
    echo "Examples:"
    echo "  $0 inflate_harness                    # Run inflate harness for 60s"
    echo "  $0 -t 300 deflate_harness             # Run deflate harness for 5 minutes"
    echo "  $0 --all -t 120                       # Run all harnesses for 2 minutes each"
    echo "  $0 --parallel -w 4 -t 600             # Run all harnesses in parallel for 10 minutes"
}

list_harnesses() {
    echo "📋 Available harnesses in $HARNESS_DIR:"
    cd "$HARNESS_DIR"
    for harness in *_harness; do
        if [ -x "$harness" ]; then
            size=$(stat -c%s "$harness" 2>/dev/null || echo "unknown")
            echo "  ✅ $harness (${size} bytes)"
        fi
    done
}

setup_directories() {
    local harness_name="$1"
    
    # Create corpus directory for this harness
    mkdir -p "$CORPUS_DIR/$harness_name"
    
    # Create artifacts directory with timestamp
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local artifact_dir="$ARTIFACTS_DIR/${harness_name}_${timestamp}"
    mkdir -p "$artifact_dir"
    
    echo "$artifact_dir"
}

run_single_harness() {
    local harness_name="$1"
    local duration="$2"
    local workers="$3"
    
    echo "🎯 Running $harness_name"
    echo "   Duration: ${duration}s"
    echo "   Workers: $workers"
    
    cd "$HARNESS_DIR"
    
    if [ ! -x "$harness_name" ]; then
        echo "❌ Harness $harness_name not found or not executable"
        echo "💡 Run ./build.sh first"
        return 1
    fi
    
    # Setup directories
    local artifact_dir=$(setup_directories "$harness_name")
    
    echo "   Corpus: $CORPUS_DIR/$harness_name"
    echo "   Artifacts: $artifact_dir"
    echo ""
    
    # Run fuzzer with crash artifacts redirected
    local cmd="./$harness_name"
    cmd="$cmd -max_total_time=$duration"
    cmd="$cmd -artifact_prefix=$artifact_dir/"
    cmd="$cmd -print_final_stats=1"
    
    if [ "$workers" -gt 1 ]; then
        cmd="$cmd -workers=$workers"
    fi
    
    cmd="$cmd $CORPUS_DIR/$harness_name"
    
    echo "🚀 Executing: $cmd"
    echo "----------------------------------------"
    
    $cmd
    
    echo ""
    echo "✅ $harness_name completed"
    
    # Report artifacts
    local crash_count=$(find "$artifact_dir" -name "crash-*" | wc -l)
    local timeout_count=$(find "$artifact_dir" -name "timeout-*" | wc -l)
    local oom_count=$(find "$artifact_dir" -name "oom-*" | wc -l)
    
    echo "📊 Results:"
    echo "   Crashes: $crash_count"
    echo "   Timeouts: $timeout_count"  
    echo "   OOMs: $oom_count"
    
    if [ $crash_count -gt 0 ]; then
        echo "💥 Crash artifacts found in: $artifact_dir"
    fi
    
    echo ""
}

run_all_sequential() {
    local duration="$1"
    
    echo "🔄 Running all harnesses sequentially"
    echo "Duration per harness: ${duration}s"
    echo ""
    
    local harnesses=(
        "deflateInit_harness"
        "deflate_harness"
        "deflateEnd_harness" 
        "inflateInit_harness"
        "inflate_harness"
        "inflateEnd_harness"
        "compress_harness"
        "uncompress_harness"
    )
    
    for harness in "${harnesses[@]}"; do
        run_single_harness "$harness" "$duration" 1
        echo "⏸️  Pausing 2 seconds..."
        sleep 2
    done
    
    echo "🎉 All harnesses completed!"
}

run_all_parallel() {
    local duration="$1"
    local workers="$2"
    
    echo "⚡ Running all harnesses in parallel"
    echo "Duration: ${duration}s per harness"
    echo "Workers: $workers per harness"
    echo ""
    
    local harnesses=(
        "deflateInit_harness"
        "deflate_harness"
        "deflateEnd_harness"
        "inflateInit_harness"
        "inflate_harness" 
        "inflateEnd_harness"
        "compress_harness"
        "uncompress_harness"
    )
    
    # Start all harnesses in background
    local pids=()
    for harness in "${harnesses[@]}"; do
        (run_single_harness "$harness" "$duration" "$workers") &
        pids+=($!)
        echo "Started $harness (PID: $!)"
    done
    
    echo ""
    echo "⏳ Waiting for all harnesses to complete..."
    
    # Wait for all to complete
    for pid in "${pids[@]}"; do
        wait $pid
    done
    
    echo ""
    echo "🎉 All parallel harnesses completed!"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--time)
            DURATION="$2"
            shift 2
            ;;
        -w|--workers)
            WORKERS="$2"
            shift 2
            ;;
        -a|--all)
            MODE="all"
            shift
            ;;
        -p|--parallel)
            MODE="parallel"
            shift
            ;;
        -l|--list)
            list_harnesses
            exit 0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            if [ -z "$HARNESS" ]; then
                HARNESS="$1"
            else
                echo "❌ Unknown option: $1"
                usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate inputs
if ! [[ "$DURATION" =~ ^[0-9]+$ ]]; then
    echo "❌ Invalid duration: $DURATION"
    exit 1
fi

if ! [[ "$WORKERS" =~ ^[0-9]+$ ]] || [ "$WORKERS" -lt 1 ]; then
    echo "❌ Invalid workers count: $WORKERS"
    exit 1
fi

# Create base directories
mkdir -p "$CORPUS_DIR" "$ARTIFACTS_DIR"

echo "📁 Setup:"
echo "  Script: $SCRIPT_DIR"
echo "  Harnesses: $HARNESS_DIR"
echo "  Corpus: $CORPUS_DIR"
echo "  Artifacts: $ARTIFACTS_DIR"
echo ""

# Execute based on mode
case $MODE in
    "single")
        if [ -z "$HARNESS" ]; then
            echo "❌ No harness specified for single mode"
            usage
            exit 1
        fi
        run_single_harness "$HARNESS" "$DURATION" "$WORKERS"
        ;;
    "all")
        run_all_sequential "$DURATION"
        ;;
    "parallel")
        run_all_parallel "$DURATION" "$WORKERS"
        ;;
    *)
        echo "❌ Invalid mode: $MODE"
        exit 1
        ;;
esac

echo ""
echo "✨ Fuzzing session completed!"
echo "📁 Check artifacts in: $ARTIFACTS_DIR"