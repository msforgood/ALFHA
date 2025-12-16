#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATEPARAMS 0x12

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 6) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t input_buf[512];
    uint8_t output_buf[1024];
    
    // Extract parameters from input data
    int new_level = (int8_t)data[0]; // Allow negative values
    int new_strategy = (int8_t)data[1]; // Allow negative values
    int initial_level = (data[2] % 10); // 0-9
    size_t input_len = size > 6 ? ((size - 6) < sizeof(input_buf) ? (size - 6) : sizeof(input_buf)) : 0;
    
    if (input_len > 0) {
        memcpy(input_buf, data + 6, input_len);
    }
    
    // Test deflateParams with NULL pointer (should fail gracefully)
    ret = deflateParams(NULL, new_level, new_strategy);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test deflateParams on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateParams(&strm, new_level, new_strategy);
    // Should fail since stream is not initialized
    
    // Proper test: Initialize, compress some data, then change parameters
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, initial_level);
    
    if (ret == Z_OK) {
        // Do some initial compression if we have input data
        if (input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len / 2; // Use half for initial compression
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            ret = deflate(&strm, Z_NO_FLUSH);
        }
        
        // Test deflateParams with various level/strategy combinations
        int test_levels[] = {Z_DEFAULT_COMPRESSION, 0, 1, 6, 9, -1, 10, 100};
        int test_strategies[] = {Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED, -1, 5, 255};
        
        for (int i = 0; i < 3 && i < 8; i++) { // Limit iterations
            ret = deflateParams(&strm, test_levels[i % 8], test_strategies[i % 8]);
            
            if (ret == Z_OK) {
                // Continue compression with new parameters
                if (input_len > 0) {
                    strm.next_in = input_buf + (input_len / 2);
                    strm.avail_in = input_len - (input_len / 2);
                    strm.next_out = output_buf + (sizeof(output_buf) - strm.avail_out);
                    strm.avail_out = strm.avail_out;
                    
                    ret = deflate(&strm, Z_NO_FLUSH);
                }
            }
        }
        
        deflateEnd(&strm);
    }
    
    // Test deflateParams with user-provided parameters
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    
    if (ret == Z_OK) {
        // Set up initial compression
        if (input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            ret = deflate(&strm, Z_NO_FLUSH);
        }
        
        // Change parameters using user input
        ret = deflateParams(&strm, new_level, new_strategy);
        
        if (ret == Z_OK) {
            // Continue compression with new parameters
            if (strm.avail_in > 0) {
                ret = deflate(&strm, Z_FINISH);
            }
        }
        
        deflateEnd(&strm);
    }
    
    // Test parameter changes at different compression stages
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, 1);
    
    if (ret == Z_OK && input_len > 0) {
        strm.next_in = input_buf;
        strm.avail_in = input_len;
        strm.next_out = output_buf;
        strm.avail_out = sizeof(output_buf);
        
        // Change parameters before any compression
        ret = deflateParams(&strm, 9, Z_HUFFMAN_ONLY);
        if (ret == Z_OK) {
            ret = deflate(&strm, Z_NO_FLUSH);
        }
        
        // Change parameters in middle of compression
        if (strm.avail_in > 0) {
            ret = deflateParams(&strm, 1, Z_RLE);
            if (ret == Z_OK) {
                ret = deflate(&strm, Z_NO_FLUSH);
            }
        }
        
        // Change parameters before final flush
        ret = deflateParams(&strm, 6, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            ret = deflate(&strm, Z_FINISH);
        }
        
        deflateEnd(&strm);
    }
    
    // Test deflateParams after deflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        deflateEnd(&strm);
        
        // Try params change after end - should fail
        ret = deflateParams(&strm, new_level, new_strategy);
    }
    
    // Test extreme parameter values
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    
    if (ret == Z_OK) {
        int extreme_params[][2] = {
            {-32768, -32768},
            {32767, 32767}, 
            {0, 0},
            {9, 4}, // Max valid values
            {-1, -1}, // Default values
            {10, 5}, // Just beyond valid range
        };
        
        for (int i = 0; i < 6; i++) {
            ret = deflateParams(&strm, extreme_params[i][0], extreme_params[i][1]);
            // Should either work or return appropriate error
        }
        
        deflateEnd(&strm);
    }
    
    // Test rapid parameter changes
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    
    if (ret == Z_OK) {
        // Rapid successive parameter changes
        for (int i = 0; i < 10; i++) {
            int level = (data[i % size] % 10);
            int strategy = (data[(i + 1) % size] % 5);
            ret = deflateParams(&strm, level, strategy);
            
            if (ret != Z_OK && ret != Z_BUF_ERROR) {
                break; // Stop on serious errors
            }
        }
        
        deflateEnd(&strm);
    }
    
    // Test parameter changes with different initialization types
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    
    if (ret == Z_OK) {
        // Change parameters on stream initialized with deflateInit2
        ret = deflateParams(&strm, new_level, new_strategy);
        
        if (ret == Z_OK && input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            ret = deflate(&strm, Z_FINISH);
        }
        
        deflateEnd(&strm);
    }
    
    return 0;
}