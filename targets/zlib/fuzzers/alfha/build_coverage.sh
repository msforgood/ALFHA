#!/bin/bash

# ALFHA zlib Coverage Build Script
# Builds harnesses with coverage instrumentation for llvm-cov

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="$SCRIPT_DIR/../../target"
HARNESS_DIR="$SCRIPT_DIR/harnesses"
COV_OUT_DIR="$SCRIPT_DIR/out_cov"

echo "🔍 ALFHA zlib Coverage Build Script"
echo "===================================="

# Create coverage output directory
mkdir -p "$COV_OUT_DIR"

# Check if zlib is built for coverage
if [ ! -f "$TARGET_DIR/libz.a" ]; then
    echo "❌ zlib library not found. Building zlib first..."
    cd "$TARGET_DIR"
    
    # Clean previous build
    make clean || true
    
    # Configure and build with coverage flags
    CC="clang" \
    CFLAGS="-fprofile-instr-generate -fcoverage-mapping -O2 -g" \
    ./configure
    
    make CC="clang" CFLAGS="-fprofile-instr-generate -fcoverage-mapping -O2 -g"
    
    cd "$SCRIPT_DIR"
    echo "✅ zlib library built with coverage instrumentation"
fi

echo ""
echo "📁 Target directory: $TARGET_DIR"
echo "📁 Harness directory: $HARNESS_DIR"
echo "📁 Coverage output: $COV_OUT_DIR"
echo ""

# Build harnesses with coverage
cd "$HARNESS_DIR"

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

# Coverage build flags - instrument both fuzzer and library
COV_FLAGS="-fprofile-instr-generate -fcoverage-mapping -fsanitize=fuzzer -I$TARGET_DIR -L$TARGET_DIR -lz -O2 -g"

echo "🔧 Building harnesses with coverage instrumentation..."
echo "Flags: $COV_FLAGS"
echo ""

SUCCESS_COUNT=0
FAIL_COUNT=0

for harness in "${HARNESSES[@]}"; do
    if [ -f "${harness}.c" ]; then
        echo -n "Building $harness with coverage... "
        
        if clang $COV_FLAGS "${harness}.c" -o "$COV_OUT_DIR/$harness" 2>/dev/null; then
            echo "✅"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            echo "❌"
            echo "  Error details:"
            clang $COV_FLAGS "${harness}.c" -o "$COV_OUT_DIR/$harness" 2>&1 | sed 's/^/    /'
            FAIL_COUNT=$((FAIL_COUNT + 1))
        fi
    else
        echo "⚠️  Skipping $harness (source not found)"
    fi
done

echo ""
echo "📊 Coverage Build Summary:"
echo "  ✅ Success: $SUCCESS_COUNT"
echo "  ❌ Failed:  $FAIL_COUNT"

if [ $FAIL_COUNT -eq 0 ]; then
    echo ""
    echo "🎉 All coverage harnesses built successfully!"
    echo ""
    echo "📋 Built coverage executables:"
    ls -la "$COV_OUT_DIR"/ | grep -E "^-rwx.*harness$" | awk '{print "  " $9 " (" $5 " bytes)"}'
    echo ""
    echo "🚀 Ready to run coverage: ./coverage_report.sh [harness_name] [corpus_dir] [seconds]"
    echo ""
    echo "📊 Example usage:"
    echo "  ./coverage_report.sh compress_harness ./corpus/compress_harness 30"
    echo "  ./coverage_report.sh inflate_harness ./corpus/inflate_harness 60"
else
    echo ""
    echo "⚠️  Some builds failed. Check error messages above."
    exit 1
fi