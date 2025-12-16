#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATESETDICTIONARY 0x03

#define MAX_DICT_SIZE 65536

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(uInt)) {
        return 0;
    }
    
    z_stream strm;
    Bytef *dictionary = NULL;
    uInt dictLength;
    int ret;
    
    // Extract dictionary length from input data
    memcpy(&dictLength, data, sizeof(uInt));
    
    // Test with NULL stream pointer (should fail gracefully)
    if (size > sizeof(uInt)) {
        ret = deflateSetDictionary(NULL, data + sizeof(uInt), 
                                  size - sizeof(uInt) > 1000 ? 1000 : size - sizeof(uInt));
        if (ret != Z_STREAM_ERROR) {
            // Unexpected behavior for NULL stream
        }
    }
    
    // Test with uninitialized stream (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateSetDictionary(&strm, data + sizeof(uInt), 100);
    if (ret != Z_STREAM_ERROR) {
        // Should fail with uninitialized stream
    }
    
    // Test with NULL dictionary pointer
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        ret = deflateSetDictionary(&strm, NULL, dictLength);
        if (ret != Z_STREAM_ERROR) {
            // Should fail with NULL dictionary
        }
        deflateEnd(&strm);
    }
    
    // Test with zero dictionary length
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        if (size > sizeof(uInt)) {
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), 0);
        }
        deflateEnd(&strm);
    }
    
    // Test with valid dictionary for zlib format
    if (size > sizeof(uInt) + 10) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            uInt dict_size = size - sizeof(uInt);
            if (dict_size > MAX_DICT_SIZE) dict_size = MAX_DICT_SIZE;
            
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), dict_size);
            deflateEnd(&strm);
        }
    }
    
    // Test with valid dictionary for raw deflate format
    if (size > sizeof(uInt) + 10) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            uInt dict_size = size - sizeof(uInt);
            if (dict_size > MAX_DICT_SIZE) dict_size = MAX_DICT_SIZE;
            
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), dict_size);
            deflateEnd(&strm);
        }
    }
    
    // Test with gzip format (should fail - gzip doesn't support preset dictionaries)
    if (size > sizeof(uInt) + 10) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            uInt dict_size = size - sizeof(uInt);
            if (dict_size > 100) dict_size = 100;
            
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), dict_size);
            // This should fail for gzip format
            deflateEnd(&strm);
        }
    }
    
    // Test various dictionary sizes including boundary conditions
    uInt test_dict_sizes[] = {
        1, 16, 256, 1024, 4096, 16384, 32768, 65536,
        0xFFFF, 0x10000, 0x20000, 0xFFFFFFFF
    };
    
    for (int i = 0; i < sizeof(test_dict_sizes)/sizeof(test_dict_sizes[0]); i++) {
        if (size <= sizeof(uInt)) continue;
        
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            uInt actual_size = test_dict_sizes[i];
            if (actual_size > size - sizeof(uInt)) {
                actual_size = size - sizeof(uInt);
            }
            
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), actual_size);
            deflateEnd(&strm);
        }
    }
    
    // Test with dictionary larger than window size
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 9, 8, Z_DEFAULT_STRATEGY); // Small window
    if (ret == Z_OK && size > sizeof(uInt) + 1000) {
        ret = deflateSetDictionary(&strm, data + sizeof(uInt), 1000); // Should truncate
        deflateEnd(&strm);
    }
    
    // Test different dictionary content patterns
    dictionary = (Bytef*)malloc(4096);
    if (dictionary) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Pattern 1: All zeros
            memset(dictionary, 0, 4096);
            ret = deflateSetDictionary(&strm, dictionary, 4096);
            
            deflateEnd(&strm);
        }
        
        // Pattern 2: All 0xFF
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            memset(dictionary, 0xFF, 4096);
            ret = deflateSetDictionary(&strm, dictionary, 4096);
            deflateEnd(&strm);
        }
        
        // Pattern 3: Repeating pattern
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            for (int i = 0; i < 4096; i++) {
                dictionary[i] = i % 256;
            }
            ret = deflateSetDictionary(&strm, dictionary, 4096);
            deflateEnd(&strm);
        }
        
        // Pattern 4: Random data from input
        if (size > sizeof(uInt) + 1000) {
            memset(&strm, 0, sizeof(z_stream));
            ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
            if (ret == Z_OK) {
                memcpy(dictionary, data + sizeof(uInt), 
                       size - sizeof(uInt) > 4096 ? 4096 : size - sizeof(uInt));
                ret = deflateSetDictionary(&strm, dictionary, 
                                         size - sizeof(uInt) > 4096 ? 4096 : size - sizeof(uInt));
                deflateEnd(&strm);
            }
        }
        
        free(dictionary);
    }
    
    // Test timing violations - call after deflate() has started
    if (size > sizeof(uInt) + 100) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Set up input/output buffers
            strm.next_in = (Bytef*)(data + sizeof(uInt));
            strm.avail_in = 50;
            
            Bytef output_buffer[512];
            strm.next_out = output_buffer;
            strm.avail_out = 512;
            
            // Call deflate first
            ret = deflate(&strm, Z_NO_FLUSH);
            
            // Now try to set dictionary (should fail for zlib format)
            ret = deflateSetDictionary(&strm, data + sizeof(uInt) + 50, 50);
            
            deflateEnd(&strm);
        }
    }
    
    // Test multiple dictionary setting attempts
    if (size > sizeof(uInt) + 200) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY); // Raw deflate
        if (ret == Z_OK) {
            // First dictionary
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), 100);
            
            // Second dictionary (should overwrite the first for raw deflate)
            ret = deflateSetDictionary(&strm, data + sizeof(uInt) + 100, 100);
            
            deflateEnd(&strm);
        }
    }
    
    // Test with different window sizes and corresponding dictionary sizes
    int window_sizes[] = {9, 12, 15};
    for (int w = 0; w < 3; w++) {
        if (size <= sizeof(uInt) + 100) continue;
        
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          window_sizes[w], 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Dictionary size based on window size (maximum effective size)
            uInt max_dict = (1 << window_sizes[w]) - 262;
            uInt dict_size = size - sizeof(uInt);
            if (dict_size > max_dict) dict_size = max_dict;
            if (dict_size > 1000) dict_size = 1000; // Keep reasonable for fuzzing
            
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), dict_size);
            deflateEnd(&strm);
        }
    }
    
    // Test with corrupted stream state
    if (size > sizeof(uInt) + 100) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Corrupt some internal state (simulate memory corruption)
            if (size >= sizeof(uInt) + sizeof(void*)) {
                // Save original state pointer
                void *original_state = strm.state;
                
                // Temporarily corrupt state pointer with data from input
                memcpy(&strm.state, data + sizeof(uInt), sizeof(void*));
                
                ret = deflateSetDictionary(&strm, data + sizeof(uInt) + sizeof(void*), 50);
                
                // Restore original state pointer for cleanup
                strm.state = original_state;
            }
            deflateEnd(&strm);
        }
    }
    
    // Test dictionary size overflow scenarios
    if (size > sizeof(uInt) + 100) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Test with extremely large dictionary length that might cause integer overflow
            uInt overflow_size = 0xFFFFFFFF;
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), overflow_size);
            deflateEnd(&strm);
        }
    }
    
    // Test with different compression strategies and dictionaries
    int strategies[] = {Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED};
    for (int s = 0; s < 5; s++) {
        if (size <= sizeof(uInt) + 50) continue;
        
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          15, 8, strategies[s]);
        if (ret == Z_OK) {
            ret = deflateSetDictionary(&strm, data + sizeof(uInt), 50);
            deflateEnd(&strm);
        }
    }
    
    return 0;
}