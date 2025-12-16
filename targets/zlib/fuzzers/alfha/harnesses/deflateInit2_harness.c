#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATEINIT2 0x02

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5 * sizeof(int)) {
        return 0;
    }
    
    z_stream strm;
    int level, method, windowBits, memLevel, strategy;
    int ret;
    
    // Extract parameters from input data
    memcpy(&level, data, sizeof(int));
    memcpy(&method, data + sizeof(int), sizeof(int));
    memcpy(&windowBits, data + 2 * sizeof(int), sizeof(int));
    memcpy(&memLevel, data + 3 * sizeof(int), sizeof(int));
    memcpy(&strategy, data + 4 * sizeof(int), sizeof(int));
    
    // Test with NULL pointer (should fail gracefully)
    ret = deflateInit2(NULL, level, method, windowBits, memLevel, strategy);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL stream
    }
    
    // Test with uninitialized stream structure
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit2(&strm, level, method, windowBits, memLevel, strategy);
    
    if (ret == Z_OK) {
        // Successful initialization, clean up
        deflateEnd(&strm);
    }
    
    // Test all compression levels including invalid ones
    int test_levels[] = {
        Z_DEFAULT_COMPRESSION, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        -2, -10, 10, 100, 32767, -32768
    };
    
    for (int i = 0; i < sizeof(test_levels)/sizeof(test_levels[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, test_levels[i], Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test various method values including invalid ones
    int test_methods[] = {
        Z_DEFLATED, 0, 1, 2, 99, -1, 256
    };
    
    for (int i = 0; i < sizeof(test_methods)/sizeof(test_methods[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, test_methods[i], 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test windowBits boundary conditions and different formats
    int test_windowBits[] = {
        // Valid zlib format (8-15)
        8, 9, 10, 15,
        // Raw deflate format (-8 to -15) - note -8 is invalid
        -9, -10, -15,
        // Gzip format (24-31)
        24, 25, 31,
        // Invalid values
        7, 16, -7, -8, -16, 32, 0, 1, 100, -100
    };
    
    for (int i = 0; i < sizeof(test_windowBits)/sizeof(test_windowBits[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          test_windowBits[i], 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test memLevel boundary conditions
    int test_memLevels[] = {
        1, 2, 8, 9, 0, 10, 100, -1
    };
    
    for (int i = 0; i < sizeof(test_memLevels)/sizeof(test_memLevels[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          15, test_memLevels[i], Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test all compression strategies including invalid ones
    int test_strategies[] = {
        Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE,
        Z_FIXED, -1, 10, 100
    };
    
    for (int i = 0; i < sizeof(test_strategies)/sizeof(test_strategies[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          15, 8, test_strategies[i]);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test pre-initialized structure with custom allocators
    if (size >= 6 * sizeof(int)) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Simulate different allocator configurations from input
        if (data[5 * sizeof(int)] & 0x01) {
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
        }
        
        if (data[5 * sizeof(int)] & 0x02) {
            strm.opaque = (voidpf)&strm; // Use safe pointer
        }
        
        ret = deflateInit2(&strm, level, method, windowBits, memLevel, strategy);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test edge case combinations that might cause integer overflows
    struct {
        int level, method, windowBits, memLevel, strategy;
    } edge_cases[] = {
        // Maximum valid values
        {9, Z_DEFLATED, 15, 9, Z_FIXED},
        {9, Z_DEFLATED, 31, 9, Z_FIXED}, // gzip format
        {9, Z_DEFLATED, -15, 9, Z_FIXED}, // raw deflate
        // Minimum valid values
        {0, Z_DEFLATED, 9, 1, Z_DEFAULT_STRATEGY},
        {0, Z_DEFLATED, -9, 1, Z_DEFAULT_STRATEGY},
        // Mixed valid/invalid combinations
        {32767, Z_DEFLATED, 15, 9, Z_FIXED},
        {9, Z_DEFLATED, 32767, 9, Z_FIXED},
        {9, Z_DEFLATED, 15, 32767, Z_FIXED},
        {9, Z_DEFLATED, 15, 9, 32767},
    };
    
    for (int i = 0; i < sizeof(edge_cases)/sizeof(edge_cases[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, edge_cases[i].level, edge_cases[i].method,
                          edge_cases[i].windowBits, edge_cases[i].memLevel,
                          edge_cases[i].strategy);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test with corrupted stream structure (but still allocated)
    if (size >= 20) {
        memset(&strm, 0, sizeof(z_stream));
        // Introduce some corruption from input data
        memcpy(&strm, data + 5 * sizeof(int), 
               size - 5 * sizeof(int) > sizeof(z_stream) ? sizeof(z_stream) : size - 5 * sizeof(int));
        
        // Reset critical pointers to safe values to avoid crashes
        strm.next_in = Z_NULL;
        strm.next_out = Z_NULL;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        ret = deflateInit2(&strm, level, method, windowBits, memLevel, strategy);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test parameter combinations that stress memory allocation
    int stress_windowBits[] = {15, 31, -15}; // Different formats requiring different memory
    int stress_memLevels[] = {1, 9}; // Extreme memory levels
    
    for (int w = 0; w < 3; w++) {
        for (int m = 0; m < 2; m++) {
            for (int s = 0; s < 3; s++) { // Multiple strategy types
                memset(&strm, 0, sizeof(z_stream));
                ret = deflateInit2(&strm, 9, Z_DEFLATED, stress_windowBits[w], 
                                  stress_memLevels[m], test_strategies[s]);
                if (ret == Z_OK) {
                    deflateEnd(&strm);
                }
            }
        }
    }
    
    // Test sequential initialization and cleanup to check for state leaks
    for (int seq = 0; seq < 5; seq++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test failure recovery - initialize, fail, try again
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit2(&strm, 100, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY); // Should fail
    if (ret != Z_OK) {
        // Try again with valid parameters
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    } else {
        deflateEnd(&strm);
    }
    
    return 0;
}