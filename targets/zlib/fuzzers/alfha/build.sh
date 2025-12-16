#!/bin/bash

# ALFHA zlib Fuzzer Build Script
# Builds all critical priority function harnesses

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="$SCRIPT_DIR/../../target"
HARNESS_DIR="$SCRIPT_DIR/harnesses"

echo "🔨 ALFHA zlib Fuzzer Build Script"
echo "=================================="

# Check if zlib is built
if [ ! -f "$TARGET_DIR/libz.a" ]; then
    echo "❌ zlib library not found. Building zlib first..."
    cd "$TARGET_DIR"
    ./configure
    make
    cd "$SCRIPT_DIR"
    echo "✅ zlib library built successfully"
fi

echo ""
echo "📁 Target directory: $TARGET_DIR"
echo "📁 Harness directory: $HARNESS_DIR"
echo ""

# Build harnesses
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
)

BUILD_FLAGS="-fsanitize=fuzzer -I$TARGET_DIR -L$TARGET_DIR -lz -O2 -g"

echo "🔧 Building harnesses with clang..."
echo "Flags: $BUILD_FLAGS"
echo ""

SUCCESS_COUNT=0
FAIL_COUNT=0

for harness in "${HARNESSES[@]}"; do
    echo -n "Building $harness... "
    
    if clang $BUILD_FLAGS "${harness}.c" -o "$harness" 2>/dev/null; then
        echo "✅"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo "❌"
        echo "  Error details:"
        clang $BUILD_FLAGS "${harness}.c" -o "$harness" 2>&1 | sed 's/^/    /'
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

echo ""
echo "📊 Build Summary:"
echo "  ✅ Success: $SUCCESS_COUNT"
echo "  ❌ Failed:  $FAIL_COUNT"

if [ $FAIL_COUNT -eq 0 ]; then
    echo ""
    echo "🎉 All harnesses built successfully!"
    echo ""
    echo "📋 Built executables:"
    ls -la | grep -E "^-rwx.*harness$" | awk '{print "  " $9 " (" $5 " bytes)"}'
    echo ""
    echo "🚀 Ready to run: ./run.sh"
else
    echo ""
    echo "⚠️  Some builds failed. Check error messages above."
    exit 1
fi