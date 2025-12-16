#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATERESET 0x10

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t input_buf[256];
    uint8_t output_buf[512];
    
    // Extract initial compression level and test data
    int level = (data[0] % 10) + 1; // Level 1-9
    size_t input_len = size > 4 ? ((size - 4) < sizeof(input_buf) ? (size - 4) : sizeof(input_buf)) : 0;
    
    if (input_len > 0) {
        memcpy(input_buf, data + 4, input_len);
    }
    
    // Test deflateReset with NULL pointer (should fail gracefully)
    ret = deflateReset(NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior but continue
    }
    
    // Test deflateReset on uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateReset(&strm);
    // Should fail since stream is not initialized
    
    // Proper test sequence: Init -> Use -> Reset -> Use again
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, level);
    
    if (ret == Z_OK) {
        // First compression operation
        if (input_len > 0) {
            strm.next_in = input_buf;
            strm.avail_in = input_len;
            strm.next_out = output_buf;
            strm.avail_out = sizeof(output_buf);
            
            ret = deflate(&strm, Z_FINISH);
            // Don't check result, continue testing
        }
        
        // Test deflateReset on active stream
        ret = deflateReset(&strm);
        
        if (ret == Z_OK) {
            // Verify stream is reset by trying another compression
            if (input_len > 0) {
                // Reset buffer positions 
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                
                // Should work after reset
                ret = deflate(&strm, Z_FINISH);
            }
        }
        
        deflateEnd(&strm);
    }
    
    // Test deflateReset after deflateEnd (should fail)
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        deflateEnd(&strm);
        // Try reset after end - should fail
        ret = deflateReset(&strm);
    }
    
    // Test multiple resets in sequence
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, level);
    if (ret == Z_OK) {
        // Multiple consecutive resets
        for (int i = 0; i < 5; i++) {
            ret = deflateReset(&strm);
            if (ret != Z_OK) break;
        }
        deflateEnd(&strm);
    }
    
    // Test reset with corrupted stream state
    if (size >= 8) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        
        if (ret == Z_OK) {
            // Simulate some compression activity first
            if (input_len > 0) {
                strm.next_in = input_buf;
                strm.avail_in = input_len / 2;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                deflate(&strm, Z_NO_FLUSH);
            }
            
            // Corrupt some internal state fields (but keep it safe)
            uInt orig_avail_in = strm.avail_in;
            uLong orig_total_in = strm.total_in;
            
            // Use input data to corrupt state
            memcpy(&strm.avail_in, data, sizeof(uInt));
            memcpy(&strm.total_in, data + sizeof(uInt), sizeof(uLong));
            
            // Try reset with corrupted state
            ret = deflateReset(&strm);
            
            // Reset should restore proper state regardless
            if (ret == Z_OK) {
                // Verify basic functionality after reset
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                deflate(&strm, Z_FINISH);
            }
            
            deflateEnd(&strm);
        }
    }
    
    // Test reset with different compression levels
    int test_levels[] = {1, 6, 9, Z_DEFAULT_COMPRESSION};
    for (int i = 0; i < 4; i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, test_levels[i]);
        
        if (ret == Z_OK) {
            // Do some compression
            if (input_len > 0) {
                strm.next_in = input_buf;
                strm.avail_in = input_len;
                strm.next_out = output_buf;
                strm.avail_out = sizeof(output_buf);
                deflate(&strm, Z_NO_FLUSH);
            }
            
            // Reset and verify level is preserved
            ret = deflateReset(&strm);
            if (ret == Z_OK) {
                // Continue compression to verify state
                if (input_len > 0) {
                    strm.next_in = input_buf;
                    strm.avail_in = input_len;
                    strm.next_out = output_buf;
                    strm.avail_out = sizeof(output_buf);
                    deflate(&strm, Z_FINISH);
                }
            }
            
            deflateEnd(&strm);
        }
    }
    
    return 0;
}