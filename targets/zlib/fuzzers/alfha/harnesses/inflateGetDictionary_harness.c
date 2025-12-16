#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATEGETDICTIONARY 0x19

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t dictionary_buf[512];
    uint8_t input_buf[256];
    uint8_t output_buf[512];
    uint8_t test_dict[128];
    uInt dict_length;
    
    // Extract parameters from input data
    size_t dict_buf_size = (data[0] % 4) * 128; // 0, 128, 256, 384 bytes
    if (dict_buf_size > sizeof(dictionary_buf)) dict_buf_size = sizeof(dictionary_buf);
    
    size_t input_len = size > 4 ? ((size - 4) < sizeof(input_buf) ? (size - 4) : sizeof(input_buf)) : 0;
    if (input_len > 0) {
        memcpy(input_buf, data + 4, input_len);
        // Also use part of input as test dictionary
        size_t test_dict_len = input_len > sizeof(test_dict) ? sizeof(test_dict) : input_len;
        memcpy(test_dict, data + 4, test_dict_len);
    }
    
    // Test inflateGetDictionary with NULL stream (should fail gracefully)
    dict_length = dict_buf_size;
    ret = inflateGetDictionary(NULL, dictionary_buf, &dict_length);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test inflateGetDictionary on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    dict_length = dict_buf_size;
    ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
    // Should fail since stream is not initialized
    
    // Test with NULL dictionary buffer
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        dict_length = dict_buf_size;
        ret = inflateGetDictionary(&strm, NULL, &dict_length);
        // Should handle NULL buffer gracefully
        
        inflateEnd(&strm);
    }
    
    // Test with NULL length pointer
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        ret = inflateGetDictionary(&strm, dictionary_buf, NULL);
        // Should fail with NULL length pointer
        
        inflateEnd(&strm);
    }
    
    // Proper test: Initialize stream without dictionary first
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    
    if (ret == Z_OK) {
        // Try to get dictionary when none is set
        dict_length = dict_buf_size;
        memset(dictionary_buf, 0xAA, sizeof(dictionary_buf)); // Fill with pattern
        
        ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
        // Should return appropriate result (no dictionary set)
        
        inflateEnd(&strm);
    }
    
    // Test with stream that has a dictionary set
    if (input_len >= 32) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            // Set a dictionary using input data
            size_t dict_set_size = (input_len > 64) ? 64 : input_len;
            ret = inflateSetDictionary(&strm, input_buf, dict_set_size);
            
            if (ret == Z_OK) {
                // Now try to get the dictionary back
                dict_length = dict_buf_size;
                memset(dictionary_buf, 0x55, sizeof(dictionary_buf)); // Clear with pattern
                
                ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
                
                if (ret == Z_OK) {
                    // Verify the retrieved dictionary matches what we set
                    // (Implementation detail - may or may not match exactly)
                }
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test with insufficient buffer size
    if (input_len >= 16) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            // Set a larger dictionary
            ret = inflateSetDictionary(&strm, input_buf, input_len);
            
            if (ret == Z_OK) {
                // Try to get dictionary with very small buffer
                dict_length = 1; // Very small buffer
                ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
                // Should handle small buffer appropriately
                
                // Try with zero buffer size
                dict_length = 0;
                ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test after inflation has started
    if (input_len > 16) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            // Set dictionary first
            ret = inflateSetDictionary(&strm, test_dict, sizeof(test_dict));
            
            // Do some inflation (may fail with random data)
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            ret = inflate(&strm, Z_NO_FLUSH);
            // Don't check result, continue testing
            
            // Try to get dictionary after inflation has started
            dict_length = dict_buf_size;
            ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
            
            inflateEnd(&strm);
        }
    }
    
    // Test with different window sizes
    int test_windows[] = {8, 9, 12, 15, -8, -15}; // Include raw deflate
    for (int i = 0; i < 6 && input_len >= 8; i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit2(&strm, test_windows[i]);
        
        if (ret == Z_OK) {
            // Set dictionary if supported for this window size
            size_t dict_size = 32;
            if (dict_size > input_len) dict_size = input_len;
            ret = inflateSetDictionary(&strm, input_buf, dict_size);
            
            // Try to get dictionary regardless of set result
            dict_length = dict_buf_size;
            ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
            
            inflateEnd(&strm);
        }
    }
    
    // Test with various buffer sizes
    uint8_t small_buf[16];
    uint8_t large_buf[1024];
    
    for (int buf_test = 0; buf_test < 3 && input_len >= 16; buf_test++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            ret = inflateSetDictionary(&strm, input_buf, (input_len > 64) ? 64 : input_len);
            
            if (ret == Z_OK || ret == Z_DATA_ERROR) { // May fail with invalid dict
                switch (buf_test) {
                    case 0: // Small buffer
                        dict_length = sizeof(small_buf);
                        ret = inflateGetDictionary(&strm, small_buf, &dict_length);
                        break;
                    case 1: // Large buffer  
                        dict_length = sizeof(large_buf);
                        ret = inflateGetDictionary(&strm, large_buf, &dict_length);
                        break;
                    case 2: // Exact size buffer
                        dict_length = 64;
                        ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
                        break;
                }
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test with corrupted length value
    if (size >= 8) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            if (input_len >= 16) {
                ret = inflateSetDictionary(&strm, input_buf, 16);
            }
            
            // Corrupt the length value with input data
            memcpy(&dict_length, data, sizeof(uInt));
            
            // Try get dictionary with corrupted length
            ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
            // Should handle corrupted length appropriately
            
            inflateEnd(&strm);
        }
    }
    
    // Test after inflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        inflateEnd(&strm);
        
        // Try to get dictionary after end - should fail
        dict_length = dict_buf_size;
        ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
    }
    
    // Test with inflateReset between set and get
    if (input_len >= 32) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            // Set dictionary
            ret = inflateSetDictionary(&strm, input_buf, 32);
            
            if (ret == Z_OK) {
                // Reset stream
                ret = inflateReset(&strm);
                
                if (ret == Z_OK) {
                    // Try to get dictionary after reset
                    dict_length = dict_buf_size;
                    ret = inflateGetDictionary(&strm, dictionary_buf, &dict_length);
                    // Dictionary should be cleared after reset
                }
            }
            
            inflateEnd(&strm);
        }
    }
    
    return 0;
}