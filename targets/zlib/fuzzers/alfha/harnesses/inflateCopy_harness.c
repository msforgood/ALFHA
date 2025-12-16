#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATECOPY 0x11

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) {
        return 0;
    }
    
    z_stream source_strm, dest_strm;
    int ret;
    uint8_t input_buf[512];
    uint8_t output_buf[1024];
    uint8_t test_data[] = {0x78, 0x9c, 0x2b, 0x49, 0x2d, 0x2e, 0x01, 0x00, 0x04, 0x5d, 0x01, 0xc1}; // "test" compressed
    
    // Extract window bits from input data
    int windowBits = 8 + (data[0] % 8); // 8-15
    size_t test_input_len = size > 8 ? ((size - 8) < sizeof(input_buf) ? (size - 8) : sizeof(input_buf)) : 0;
    
    if (test_input_len > 0) {
        memcpy(input_buf, data + 8, test_input_len);
    }
    
    // Test inflateCopy with NULL pointers (should fail gracefully)
    ret = inflateCopy(NULL, NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    memset(&source_strm, 0, sizeof(z_stream));
    ret = inflateCopy(NULL, &source_strm);
    // Should fail - dest is NULL
    
    memset(&dest_strm, 0, sizeof(z_stream));
    ret = inflateCopy(&dest_strm, NULL);
    // Should fail - source is NULL
    
    // Test inflateCopy with uninitialized source stream
    memset(&source_strm, 0, sizeof(z_stream));
    memset(&dest_strm, 0, sizeof(z_stream));
    ret = inflateCopy(&dest_strm, &source_strm);
    // Should fail - source not initialized
    
    // Proper test: Initialize source, do some inflation, then copy
    memset(&source_strm, 0, sizeof(z_stream));
    ret = inflateInit2(&source_strm, windowBits);
    
    if (ret == Z_OK) {
        // Set up source stream with test data
        source_strm.next_in = test_data;
        source_strm.avail_in = sizeof(test_data);
        source_strm.next_out = output_buf;
        source_strm.avail_out = sizeof(output_buf);
        
        // Do partial inflation to establish some state
        ret = inflate(&source_strm, Z_NO_FLUSH);
        
        // Now test inflateCopy
        memset(&dest_strm, 0, sizeof(z_stream));
        ret = inflateCopy(&dest_strm, &source_strm);
        
        if (ret == Z_OK) {
            // Verify dest stream works by continuing inflation
            dest_strm.next_out = output_buf + (sizeof(output_buf) - dest_strm.avail_out);
            dest_strm.avail_out = sizeof(output_buf) - (sizeof(output_buf) - dest_strm.avail_out);
            
            ret = inflate(&dest_strm, Z_FINISH);
            
            inflateEnd(&dest_strm);
        }
        
        inflateEnd(&source_strm);
    }
    
    // Test copy with user input data
    if (test_input_len > 0) {
        memset(&source_strm, 0, sizeof(z_stream));
        ret = inflateInit(&source_strm);
        
        if (ret == Z_OK) {
            // Set up with user data
            source_strm.next_in = input_buf;
            source_strm.avail_in = test_input_len;
            source_strm.next_out = output_buf;
            source_strm.avail_out = sizeof(output_buf);
            
            // Try partial inflation (might fail with invalid data)
            ret = inflate(&source_strm, Z_NO_FLUSH);
            
            // Copy regardless of inflation result to test copy function
            memset(&dest_strm, 0, sizeof(z_stream));
            ret = inflateCopy(&dest_strm, &source_strm);
            
            if (ret == Z_OK) {
                inflateEnd(&dest_strm);
            }
            
            inflateEnd(&source_strm);
        }
    }
    
    // Test copy with different window sizes
    int window_sizes[] = {8, 9, 12, 15, -8, -15}; // Include raw deflate
    for (int i = 0; i < 6; i++) {
        memset(&source_strm, 0, sizeof(z_stream));
        ret = inflateInit2(&source_strm, window_sizes[i]);
        
        if (ret == Z_OK) {
            // Set up minimal state
            source_strm.next_in = test_data;
            source_strm.avail_in = sizeof(test_data);
            source_strm.next_out = output_buf;
            source_strm.avail_out = sizeof(output_buf);
            
            // Attempt to copy
            memset(&dest_strm, 0, sizeof(z_stream));
            ret = inflateCopy(&dest_strm, &source_strm);
            
            if (ret == Z_OK) {
                inflateEnd(&dest_strm);
            }
            
            inflateEnd(&source_strm);
        }
    }
    
    // Test copy after inflateEnd (should fail)
    memset(&source_strm, 0, sizeof(z_stream));
    ret = inflateInit(&source_strm);
    if (ret == Z_OK) {
        inflateEnd(&source_strm);
        
        // Try copy after end - should fail
        memset(&dest_strm, 0, sizeof(z_stream));
        ret = inflateCopy(&dest_strm, &source_strm);
    }
    
    // Test copy to already initialized destination
    memset(&source_strm, 0, sizeof(z_stream));
    memset(&dest_strm, 0, sizeof(z_stream));
    
    ret = inflateInit(&source_strm);
    if (ret == Z_OK) {
        ret = inflateInit(&dest_strm);
        if (ret == Z_OK) {
            // Copy to already initialized stream
            ret = inflateCopy(&dest_strm, &source_strm);
            // Should either work or fail gracefully
            
            inflateEnd(&dest_strm);
        }
        inflateEnd(&source_strm);
    }
    
    // Test with corrupted source stream state
    if (size >= 16) {
        memset(&source_strm, 0, sizeof(z_stream));
        ret = inflateInit(&source_strm);
        
        if (ret == Z_OK) {
            // Corrupt some non-critical fields with input data
            memcpy(&source_strm.total_in, data, sizeof(uLong));
            memcpy(&source_strm.total_out, data + sizeof(uLong), sizeof(uLong));
            
            // Try copy with corrupted state
            memset(&dest_strm, 0, sizeof(z_stream));
            ret = inflateCopy(&dest_strm, &source_strm);
            
            if (ret == Z_OK) {
                inflateEnd(&dest_strm);
            }
            
            inflateEnd(&source_strm);
        }
    }
    
    // Test multiple consecutive copies from same source
    memset(&source_strm, 0, sizeof(z_stream));
    ret = inflateInit(&source_strm);
    
    if (ret == Z_OK) {
        z_stream dest_strm1, dest_strm2, dest_strm3;
        
        // Create multiple copies
        memset(&dest_strm1, 0, sizeof(z_stream));
        ret = inflateCopy(&dest_strm1, &source_strm);
        
        if (ret == Z_OK) {
            memset(&dest_strm2, 0, sizeof(z_stream));
            ret = inflateCopy(&dest_strm2, &source_strm);
            
            if (ret == Z_OK) {
                memset(&dest_strm3, 0, sizeof(z_stream));
                ret = inflateCopy(&dest_strm3, &dest_strm1); // Copy from copy
                
                if (ret == Z_OK) {
                    inflateEnd(&dest_strm3);
                }
                inflateEnd(&dest_strm2);
            }
            inflateEnd(&dest_strm1);
        }
        
        inflateEnd(&source_strm);
    }
    
    return 0;
}