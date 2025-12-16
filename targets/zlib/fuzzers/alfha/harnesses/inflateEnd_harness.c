#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATEEND 0x06

#define MAX_BUFFER_SIZE 1024

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    z_stream strm;
    int ret;
    uint8_t output_buffer[MAX_BUFFER_SIZE];
    
    // Test with NULL pointer (should fail gracefully)
    ret = inflateEnd(NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior, but continue
    }
    
    // Test with uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateEnd(&strm);
    // Should return Z_STREAM_ERROR for uninitialized stream
    
    // Test with properly initialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        ret = inflateEnd(&strm);
        // Should return Z_OK
    }
    
    // Test with stream that has processed data
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        if (size > 0) {
            // Use input data as compressed stream
            strm.next_in = (Bytef*)data;
            strm.avail_in = size > MAX_BUFFER_SIZE ? MAX_BUFFER_SIZE : size;
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            
            // Process some data (may fail with corrupted input, that's okay)
            ret = inflate(&strm, Z_NO_FLUSH);
        }
        
        // End the stream with pending/processed data
        ret = inflateEnd(&strm);
    }
    
    // Test with stream in finished state
    if (size >= 20) { // Need some data to create valid compressed stream
        z_stream comp_strm;
        uint8_t compressed_buffer[MAX_BUFFER_SIZE];
        
        // First create some compressed data
        memset(&comp_strm, 0, sizeof(z_stream));
        ret = deflateInit(&comp_strm, Z_DEFAULT_COMPRESSION);
        if (ret == Z_OK) {
            size_t input_size = size > 256 ? 256 : size;
            
            comp_strm.next_in = (Bytef*)data;
            comp_strm.avail_in = input_size;
            comp_strm.next_out = compressed_buffer;
            comp_strm.avail_out = MAX_BUFFER_SIZE;
            
            ret = deflate(&comp_strm, Z_FINISH);
            
            if (ret == Z_STREAM_END) {
                size_t compressed_size = MAX_BUFFER_SIZE - comp_strm.avail_out;
                deflateEnd(&comp_strm);
                
                // Now inflate to completion
                memset(&strm, 0, sizeof(z_stream));
                ret = inflateInit(&strm);
                if (ret == Z_OK) {
                    strm.next_in = compressed_buffer;
                    strm.avail_in = compressed_size;
                    strm.next_out = output_buffer;
                    strm.avail_out = MAX_BUFFER_SIZE;
                    
                    // Inflate to completion
                    do {
                        ret = inflate(&strm, Z_NO_FLUSH);
                        if (strm.avail_out == 0) {
                            strm.next_out = output_buffer;
                            strm.avail_out = MAX_BUFFER_SIZE;
                        }
                    } while (ret == Z_OK);
                    
                    // Now end the finished stream
                    ret = inflateEnd(&strm);
                }
            } else {
                deflateEnd(&comp_strm);
            }
        }
    }
    
    // Test double inflateEnd (should be safe)
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        ret = inflateEnd(&strm);
        if (ret == Z_OK) {
            // Call inflateEnd again on the same stream
            ret = inflateEnd(&strm);
            // Should handle gracefully
        }
    }
    
    // Test with custom allocators
    if (size >= 4) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Simulate custom allocators scenario
        if (data[0] & 0x01) {
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
        }
        
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            ret = inflateEnd(&strm);
        }
    }
    
    // Test with various window bits settings
    int window_bits[] = { 15, -15, 15 + 16, 15 + 32 };
    for (int i = 0; i < sizeof(window_bits)/sizeof(window_bits[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit2(&strm, window_bits[i]);
        if (ret == Z_OK) {
            if (size > 0) {
                // Try to process some data
                strm.next_in = (Bytef*)data;
                strm.avail_in = size > 64 ? 64 : size;
                strm.next_out = output_buffer;
                strm.avail_out = MAX_BUFFER_SIZE;
                
                ret = inflate(&strm, Z_NO_FLUSH);
                // Ignore result, we're testing inflateEnd
            }
            
            ret = inflateEnd(&strm);
        }
    }
    
    // Test with corrupted stream state (but safely)
    if (size >= sizeof(z_stream)) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            // Corrupt some non-critical fields
            void *saved_state = strm.state;
            
            // Use input data to modify stream (but preserve critical pointers)
            size_t copy_size = size > (sizeof(z_stream) - sizeof(void*)) ? 
                             (sizeof(z_stream) - sizeof(void*)) : size;
            memcpy((char*)&strm + sizeof(void*), data, copy_size);
            
            // Restore critical state pointer
            strm.state = saved_state;
            
            ret = inflateEnd(&strm);
        }
    }
    
    // Test various stream states with different error conditions
    for (int test_case = 0; test_case < 5; test_case++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            if (size > test_case) {
                // Process varying amounts of data
                size_t process_size = (test_case + 1) * 2;
                if (process_size > size) process_size = size;
                
                strm.next_in = (Bytef*)data;
                strm.avail_in = process_size;
                strm.next_out = output_buffer;
                strm.avail_out = MAX_BUFFER_SIZE;
                
                // Try different flush modes
                int flush_modes[] = { Z_NO_FLUSH, Z_SYNC_FLUSH, Z_FINISH };
                ret = inflate(&strm, flush_modes[test_case % 3]);
            }
            
            ret = inflateEnd(&strm);
        }
    }
    
    // Test stream in error state
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        // Force an error by providing invalid data
        uint8_t invalid_data[] = { 0xFF, 0xFF, 0xFF, 0xFF };
        strm.next_in = invalid_data;
        strm.avail_in = sizeof(invalid_data);
        strm.next_out = output_buffer;
        strm.avail_out = MAX_BUFFER_SIZE;
        
        ret = inflate(&strm, Z_NO_FLUSH);
        // Stream should be in error state now
        
        ret = inflateEnd(&strm);
        // Should still clean up successfully
    }
    
    return 0;
}