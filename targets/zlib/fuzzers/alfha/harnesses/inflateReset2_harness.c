#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATERESET2 0x1C

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t input_buf[256];
    uint8_t output_buf[512];
    int windowBits;
    
    // Extract window bits and test data
    windowBits = (int8_t)data[0]; // Allow negative values for raw deflate
    size_t input_len = size > 4 ? ((size - 4) < sizeof(input_buf) ? (size - 4) : sizeof(input_buf)) : 0;
    
    if (input_len > 0) {
        memcpy(input_buf, data + 4, input_len);
    }
    
    // Test inflateReset2 with NULL pointer (should fail gracefully)
    ret = inflateReset2(NULL, windowBits);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test inflateReset2 on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateReset2(&strm, windowBits);
    // Should fail since stream is not initialized
    
    // Proper test sequence: Init -> Use -> Reset2 with new windowBits -> Use again
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm); // Initialize with default windowBits
    
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
        
        // Test inflateReset2 on active stream with new windowBits
        ret = inflateReset2(&strm, windowBits);
        
        if (ret == Z_OK) {
            // Verify stream is reset by trying another inflation
            if (input_len > 0) {
                // Reset buffer positions 
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                
                // Should work after reset with new window size
                ret = inflate(&strm, Z_NO_FLUSH);
            }
        }
        
        inflateEnd(&strm);
    }
    
    // Test inflateReset2 after inflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        inflateEnd(&strm);
        // Try reset2 after end - should fail
        ret = inflateReset2(&strm, windowBits);
    }
    
    // Test with various windowBits values
    int test_windows[] = {
        8, 9, 10, 11, 12, 13, 14, 15,    // Standard zlib windows
        -8, -9, -10, -11, -12, -13, -14, -15, // Raw deflate windows
        16+15, 32+15,                     // gzip detection
        0, 1, 7, 16, 47, 48, 64, -1, -7, -16 // Invalid values
    };
    
    for (int i = 0; i < 22; i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        
        if (ret == Z_OK) {
            // Do some initial inflation if we have data
            if (input_len > 0) {
                strm.next_in = input_buf;
                strm.avail_in = input_len / 2;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                inflate(&strm, Z_NO_FLUSH);
            }
            
            // Test reset2 with various window sizes
            ret = inflateReset2(&strm, test_windows[i]);
            
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
    
    // Test multiple reset2 calls in sequence
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    
    if (ret == Z_OK) {
        // Multiple consecutive reset2 calls with different window sizes
        int sequence_windows[] = {15, -15, 12, -12, 15};
        
        for (int i = 0; i < 5; i++) {
            ret = inflateReset2(&strm, sequence_windows[i]);
            if (ret != Z_OK) break;
            
            // Try some inflation after each reset
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
    
    // Test reset2 with corrupted stream state
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
            
            // Try reset2 with corrupted state
            ret = inflateReset2(&strm, windowBits);
            
            // Reset2 should restore proper state regardless
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
    
    // Test reset2 switching between raw deflate and zlib format
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit2(&strm, 15); // Start with zlib format
    
    if (ret == Z_OK && input_len > 0) {
        // Try some inflation
        strm.next_in = input_buf;
        strm.avail_in = input_len;
        strm.next_out = output_buf;
        strm.avail_out = sizeof(output_buf);
        inflate(&strm, Z_NO_FLUSH);
        
        // Switch to raw deflate
        ret = inflateReset2(&strm, -15);
        if (ret == Z_OK) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            inflate(&strm, Z_NO_FLUSH);
            
            // Switch back to zlib format
            ret = inflateReset2(&strm, 15);
            if (ret == Z_OK) {
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                inflate(&strm, Z_NO_FLUSH);
            }
        }
        
        inflateEnd(&strm);
    }
    
    // Test reset2 with gzip detection enabled
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit2(&strm, 15);
    
    if (ret == Z_OK) {
        // Switch to gzip detection mode
        ret = inflateReset2(&strm, 15 + 16);
        if (ret == Z_OK && input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            inflate(&strm, Z_NO_FLUSH);
        }
        
        // Switch to auto-detection mode
        ret = inflateReset2(&strm, 15 + 32);
        if (ret == Z_OK && input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            inflate(&strm, Z_NO_FLUSH);
        }
        
        inflateEnd(&strm);
    }
    
    // Test reset2 with extreme windowBits values
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    
    if (ret == Z_OK) {
        int extreme_windows[] = {
            -32768, -1000, -100, -50, 
            50, 100, 1000, 32767,
            INT_MAX, INT_MIN
        };
        
        for (int i = 0; i < 10; i++) {
            ret = inflateReset2(&strm, extreme_windows[i]);
            // Should either work or return appropriate error
        }
        
        inflateEnd(&strm);
    }
    
    return 0;
}