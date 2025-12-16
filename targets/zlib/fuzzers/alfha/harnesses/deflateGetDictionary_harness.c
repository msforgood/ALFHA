#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATEGETDICTIONARY 0x16

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t dictionary_buf[512];
    uint8_t input_buf[256];
    uint8_t output_buf[512];
    uInt dict_length;
    
    // Extract parameters from input data
    size_t dict_buf_size = (data[0] % 4) * 128; // 0, 128, 256, 384 bytes
    if (dict_buf_size > sizeof(dictionary_buf)) dict_buf_size = sizeof(dictionary_buf);
    
    size_t input_len = size > 4 ? ((size - 4) < sizeof(input_buf) ? (size - 4) : sizeof(input_buf)) : 0;
    if (input_len > 0) {
        memcpy(input_buf, data + 4, input_len);
    }
    
    // Test deflateGetDictionary with NULL stream (should fail gracefully)
    dict_length = dict_buf_size;
    ret = deflateGetDictionary(NULL, dictionary_buf, &dict_length);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test deflateGetDictionary on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    dict_length = dict_buf_size;
    ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
    // Should fail since stream is not initialized
    
    // Test with NULL dictionary buffer
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        dict_length = dict_buf_size;
        ret = deflateGetDictionary(&strm, NULL, &dict_length);
        // Should handle NULL buffer gracefully
        
        deflateEnd(&strm);
    }
    
    // Test with NULL length pointer
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        ret = deflateGetDictionary(&strm, dictionary_buf, NULL);
        // Should fail with NULL length pointer
        
        deflateEnd(&strm);
    }
    
    // Proper test: Initialize stream without dictionary first
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    
    if (ret == Z_OK) {
        // Try to get dictionary when none is set
        dict_length = dict_buf_size;
        memset(dictionary_buf, 0xAA, sizeof(dictionary_buf)); // Fill with pattern
        
        ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
        // Should return appropriate result (no dictionary set)
        
        deflateEnd(&strm);
    }
    
    // Test with stream that has a dictionary set
    if (input_len >= 32) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        
        if (ret == Z_OK) {
            // Set a dictionary using input data
            size_t dict_set_size = (input_len > 128) ? 128 : input_len;
            ret = deflateSetDictionary(&strm, input_buf, dict_set_size);
            
            if (ret == Z_OK) {
                // Now try to get the dictionary back
                dict_length = dict_buf_size;
                memset(dictionary_buf, 0x55, sizeof(dictionary_buf)); // Clear with pattern
                
                ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
                
                if (ret == Z_OK) {
                    // Verify the retrieved dictionary matches what we set
                    // (Implementation detail - may or may not match exactly)
                }
            }
            
            deflateEnd(&strm);
        }
    }
    
    // Test with insufficient buffer size
    if (input_len >= 16) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        
        if (ret == Z_OK) {
            // Set a larger dictionary
            ret = deflateSetDictionary(&strm, input_buf, input_len);
            
            if (ret == Z_OK) {
                // Try to get dictionary with very small buffer
                dict_length = 1; // Very small buffer
                ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
                // Should handle small buffer appropriately
                
                // Try with zero buffer size
                dict_length = 0;
                ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
            }
            
            deflateEnd(&strm);
        }
    }
    
    // Test after compression has started
    if (input_len > 0) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        
        if (ret == Z_OK) {
            // Set dictionary first
            if (input_len >= 32) {
                ret = deflateSetDictionary(&strm, input_buf, 32);
            }
            
            // Do some compression
            strm.next_in = input_buf + 32;
            strm.avail_in = (input_len > 32) ? (input_len - 32) : 0;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            if (strm.avail_in > 0) {
                ret = deflate(&strm, Z_NO_FLUSH);
            }
            
            // Try to get dictionary after compression has started
            dict_length = dict_buf_size;
            ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
            
            deflateEnd(&strm);
        }
    }
    
    // Test with various buffer sizes
    uint8_t small_buf[16];
    uint8_t large_buf[1024];
    
    for (int buf_test = 0; buf_test < 3; buf_test++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        
        if (ret == Z_OK && input_len >= 8) {
            ret = deflateSetDictionary(&strm, input_buf, (input_len > 64) ? 64 : input_len);
            
            if (ret == Z_OK) {
                switch (buf_test) {
                    case 0: // Small buffer
                        dict_length = sizeof(small_buf);
                        ret = deflateGetDictionary(&strm, small_buf, &dict_length);
                        break;
                    case 1: // Large buffer  
                        dict_length = sizeof(large_buf);
                        ret = deflateGetDictionary(&strm, large_buf, &dict_length);
                        break;
                    case 2: // Exact size buffer
                        dict_length = 64;
                        ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
                        break;
                }
            }
            
            deflateEnd(&strm);
        }
    }
    
    // Test with corrupted length value
    if (size >= 8) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        
        if (ret == Z_OK) {
            if (input_len >= 16) {
                ret = deflateSetDictionary(&strm, input_buf, 16);
            }
            
            if (ret == Z_OK) {
                // Corrupt the length value with input data
                memcpy(&dict_length, data, sizeof(uInt));
                
                // Try get dictionary with corrupted length
                ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
                // Should handle corrupted length appropriately
            }
            
            deflateEnd(&strm);
        }
    }
    
    // Test after deflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        deflateEnd(&strm);
        
        // Try to get dictionary after end - should fail
        dict_length = dict_buf_size;
        ret = deflateGetDictionary(&strm, dictionary_buf, &dict_length);
    }
    
    return 0;
}