#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATERESET 0x15

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t input_buf[256];
    uint8_t output_buf[512];
    
    // Extract initial window bits and test data
    int windowBits = 8 + (data[0] % 8); // 8-15
    size_t input_len = size > 4 ? ((size - 4) < sizeof(input_buf) ? (size - 4) : sizeof(input_buf)) : 0;
    
    if (input_len > 0) {
        memcpy(input_buf, data + 4, input_len);
    }
    
    // Test inflateReset with NULL pointer (should fail gracefully)
    ret = inflateReset(NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test inflateReset on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateReset(&strm);
    // Should fail since stream is not initialized
    
    // Proper test sequence: Init -> Use -> Reset -> Use again
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit2(&strm, windowBits);
    
    if (ret == Z_OK) {
        // First inflation operation with test data
        if (input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            ret = inflate(&strm, Z_NO_FLUSH);
            // Don't check result, continue testing
        }
        
        // Test inflateReset on active stream
        ret = inflateReset(&strm);
        
        if (ret == Z_OK) {
            // Verify stream is reset by trying another inflation
            if (input_len > 0) {
                // Reset buffer positions 
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                
                // Should work after reset
                ret = inflate(&strm, Z_NO_FLUSH);
            }
        }
        
        inflateEnd(&strm);
    }
    
    // Test inflateReset after inflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        inflateEnd(&strm);
        // Try reset after end - should fail
        ret = inflateReset(&strm);
    }
    
    // Test multiple resets in sequence
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        // Multiple consecutive resets
        for (int i = 0; i < 5; i++) {
            ret = inflateReset(&strm);
            if (ret != Z_OK) break;
        }
        inflateEnd(&strm);
    }
    
    // Test reset with corrupted stream state
    if (size >= 8) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            // Simulate some inflation activity first
            if (input_len > 0) {
                strm.next_in = input_buf;
                strm.avail_in = input_len / 2;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                inflate(&strm, Z_NO_FLUSH);
            }
            
            // Corrupt some internal state fields (but keep it safe)
            uInt orig_avail_in = strm.avail_in;
            uLong orig_total_in = strm.total_in;
            
            // Use input data to corrupt state
            memcpy(&strm.avail_in, data, sizeof(uInt));
            memcpy(&strm.total_in, data + sizeof(uInt), sizeof(uLong));
            
            // Try reset with corrupted state
            ret = inflateReset(&strm);
            
            // Reset should restore proper state regardless
            if (ret == Z_OK) {
                // Verify basic functionality after reset
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                inflate(&strm, Z_NO_FLUSH);
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test reset with different window sizes
    int test_windows[] = {8, 9, 12, 15, -8, -15}; // Include raw deflate
    for (int i = 0; i < 6; i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit2(&strm, test_windows[i]);
        
        if (ret == Z_OK) {
            // Do some inflation
            if (input_len > 0) {
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                inflate(&strm, Z_NO_FLUSH);
            }
            
            // Reset and verify window size is preserved
            ret = inflateReset(&strm);
            if (ret == Z_OK) {
                // Continue inflation to verify state
                if (input_len > 0) {
                    strm.next_in = input_buf;
                    strm.avail_in = input_len;
                    strm.next_out = output_buf;
                    strm.avail_out = sizeof(output_buf);
                    inflate(&strm, Z_NO_FLUSH);
                }
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test reset with partial inflation states
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK && input_len > 4) {
        // Process data in small chunks to create partial state
        strm.next_in = input_buf;
        strm.avail_in = 1; // Very small chunk
        strm.next_out = output_buf;
        strm.avail_out = sizeof(output_buf);
        
        // Try partial inflation
        ret = inflate(&strm, Z_NO_FLUSH);
        
        // Reset in middle of partial processing
        ret = inflateReset(&strm);
        
        if (ret == Z_OK) {
            // Continue with fresh data
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            inflate(&strm, Z_FINISH);
        }
        
        inflateEnd(&strm);
    }
    
    return 0;
}