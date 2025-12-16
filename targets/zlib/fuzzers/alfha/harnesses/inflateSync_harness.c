#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATESYNC 0x13

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t input_buf[1024];
    uint8_t output_buf[2048];
    
    // Extract window bits and copy input data
    int windowBits = 8 + (data[0] % 8); // 8-15
    size_t input_len = size > 4 ? ((size - 4) < sizeof(input_buf) ? (size - 4) : sizeof(input_buf)) : 0;
    
    if (input_len > 0) {
        memcpy(input_buf, data + 4, input_len);
    }
    
    // Test inflateSync with NULL pointer (should fail gracefully)
    ret = inflateSync(NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test inflateSync on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateSync(&strm);
    // Should fail since stream is not initialized
    
    // Test inflateSync on properly initialized stream with corrupted data
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit2(&strm, windowBits);
    
    if (ret == Z_OK && input_len > 0) {
        // Set up stream with user input (potentially corrupted data)
        strm.next_in = input_buf;
        strm.avail_in = input_len;
        strm.next_out = output_buf;
        strm.avail_out = sizeof(output_buf);
        
        // Try normal inflation first (likely to fail with random data)
        ret = inflate(&strm, Z_NO_FLUSH);
        
        // If inflation fails, try to sync to find valid data
        if (ret != Z_OK && ret != Z_STREAM_END) {
            ret = inflateSync(&strm);
            
            if (ret == Z_OK) {
                // Try inflation again after sync
                ret = inflate(&strm, Z_NO_FLUSH);
            }
        }
        
        inflateEnd(&strm);
    }
    
    // Test inflateSync with various types of corrupted data
    uint8_t corrupted_data[] = {
        // Random bytes that might confuse the parser
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0xAA, 0xAA, 0x55, 0x55, 0x78, 0x9C, // Some valid-looking headers mixed with garbage
        0x78, 0xDA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        // More corruption
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE
    };
    
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    
    if (ret == Z_OK) {
        strm.next_in = corrupted_data;
        strm.avail_in = sizeof(corrupted_data);
        strm.next_out = output_buf;
        strm.avail_out = sizeof(output_buf);
        
        // Try inflation with corrupted data
        ret = inflate(&strm, Z_NO_FLUSH);
        
        // Use inflateSync to try to recover
        for (int i = 0; i < 5; i++) {
            ret = inflateSync(&strm);
            if (ret != Z_OK) break;
            
            // Try continuing after each sync
            ret = inflate(&strm, Z_NO_FLUSH);
        }
        
        inflateEnd(&strm);
    }
    
    // Test inflateSync after inflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        inflateEnd(&strm);
        
        // Try sync after end - should fail
        ret = inflateSync(&strm);
    }
    
    // Test sync with mixed valid/invalid data sequences
    if (input_len > 8) {
        uint8_t mixed_data[512];
        size_t mixed_len = 0;
        
        // Create a mix of user input and known patterns
        // Add some potentially valid zlib header
        mixed_data[mixed_len++] = 0x78;
        mixed_data[mixed_len++] = 0x9C;
        
        // Add user data (corruption)
        size_t user_chunk = (input_len > 100) ? 100 : input_len;
        memcpy(mixed_data + mixed_len, input_buf, user_chunk);
        mixed_len += user_chunk;
        
        // Add more potentially valid pattern
        mixed_data[mixed_len++] = 0x01;
        mixed_data[mixed_len++] = 0x00;
        mixed_data[mixed_len++] = 0x00;
        mixed_data[mixed_len++] = 0xFF;
        mixed_data[mixed_len++] = 0xFF;
        
        // Fill rest with user data if available
        if (mixed_len < sizeof(mixed_data) && input_len > user_chunk) {
            size_t remaining = sizeof(mixed_data) - mixed_len;
            size_t available = input_len - user_chunk;
            size_t to_copy = (remaining < available) ? remaining : available;
            
            memcpy(mixed_data + mixed_len, input_buf + user_chunk, to_copy);
            mixed_len += to_copy;
        }
        
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            strm.next_in = mixed_data;
            strm.avail_in = mixed_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            // Process mixed data with sync recovery
            while (strm.avail_in > 0) {
                ret = inflate(&strm, Z_NO_FLUSH);
                
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    ret = inflateSync(&strm);
                    if (ret != Z_OK) break;
                } else {
                    break;
                }
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test inflateSync with different window sizes  
    int test_windows[] = {8, 9, 12, 15, -8, -15}; // Include raw deflate
    for (int i = 0; i < 6; i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit2(&strm, test_windows[i]);
        
        if (ret == Z_OK && input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            // Try inflation and sync
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                ret = inflateSync(&strm);
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test consecutive sync calls
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    
    if (ret == Z_OK && input_len > 0) {
        strm.next_in = input_buf;
        strm.avail_in = input_len;
        strm.next_out = output_buf;
        strm.avail_out = sizeof(output_buf);
        
        // Multiple consecutive sync attempts
        for (int i = 0; i < 10; i++) {
            ret = inflateSync(&strm);
            if (ret != Z_OK || strm.avail_in == 0) break;
        }
        
        inflateEnd(&strm);
    }
    
    return 0;
}