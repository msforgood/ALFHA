#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define FC_ZLIB_INFLATEINIT2 0x27

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(int)) {
        return 0;
    }
    
    z_stream strm;
    int windowBits;
    int ret;
    
    windowBits = ((int*)data)[0];
    
    // Test with NULL pointer (should fail gracefully)
    ret = inflateInit2(NULL, windowBits);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL stream
    }
    
    // Test with uninitialized stream
    memset(&strm, 0, sizeof(strm));
    ret = inflateInit2(&strm, windowBits);
    if (ret == Z_OK) {
        inflateEnd(&strm);
    }
    
    // Test with pre-initialized stream fields
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit2(&strm, windowBits);
    if (ret == Z_OK) {
        inflateEnd(&strm);
    }
    
    // Test various windowBits values
    int test_windowBits[] = {
        // Standard range
        8, 9, 10, 11, 12, 13, 14, 15,
        // Auto-detect
        0,
        // Raw deflate
        -8, -9, -10, -11, -12, -13, -14, -15,
        // Gzip only
        15+16,
        // Auto format detection
        15+32,
        // Invalid values
        -100, -7, -1, 7, 16, 100
    };
    
    for (int i = 0; i < sizeof(test_windowBits)/sizeof(test_windowBits[0]); i++) {
        memset(&strm, 0, sizeof(strm));
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        ret = inflateInit2(&strm, test_windowBits[i]);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test double initialization (should handle gracefully)
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit2(&strm, 15);
    if (ret == Z_OK) {
        // Try to init again without cleanup
        int ret2 = inflateInit2(&strm, 14);
        inflateEnd(&strm);
    }
    
    // Test with default allocators  
    if (size >= 2 * sizeof(int)) {
        memset(&strm, 0, sizeof(strm));
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        windowBits = ((int*)data)[1];
        ret = inflateInit2(&strm, windowBits);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test with corrupted stream structure
    if (size >= 3 * sizeof(int)) {
        memset(&strm, 0xFF, sizeof(strm)); // Fill with invalid data
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.next_in = Z_NULL;
        strm.avail_in = 0;
        strm.next_out = Z_NULL;
        strm.avail_out = 0;
        
        windowBits = ((int*)data)[2];
        ret = inflateInit2(&strm, windowBits);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test boundary values
    int boundary_values[] = {-15, -8, 8, 15, 31, 47};
    for (int i = 0; i < sizeof(boundary_values)/sizeof(boundary_values[0]); i++) {
        memset(&strm, 0, sizeof(strm));
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        ret = inflateInit2(&strm, boundary_values[i]);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    return 0;
}