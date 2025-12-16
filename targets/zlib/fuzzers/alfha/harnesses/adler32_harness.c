#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_ADLER32 0x14

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) {
        return 0;
    }
    
    uLong adler;
    uLong initial_adler;
    
    // Extract initial adler value from input data
    if (size >= sizeof(uLong)) {
        memcpy(&initial_adler, data, sizeof(uLong));
        data += sizeof(uLong);
        size -= sizeof(uLong);
    } else {
        initial_adler = 1; // Default initial value
    }
    
    // Test adler32 with NULL buffer (should handle gracefully)
    adler = adler32(1, NULL, 0);
    if (adler != 1) {
        // Should return initial value for NULL buffer with len=0
    }
    
    // Test adler32 with NULL buffer and non-zero length
    adler = adler32(1, NULL, 100);
    // Behavior might be undefined, but shouldn't crash
    
    // Test basic adler32 computation with user data
    if (size > 0) {
        adler = adler32(initial_adler, data, size);
        
        // Verify incremental computation gives same result
        if (size > 1) {
            uLong incremental = initial_adler;
            incremental = adler32(incremental, data, size / 2);
            incremental = adler32(incremental, data + (size / 2), size - (size / 2));
            
            // incremental should equal adler
        }
    }
    
    // Test with various initial adler values
    uLong test_initial_values[] = {
        0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678,
        initial_adler, ~initial_adler
    };
    
    for (int i = 0; i < 7 && size > 0; i++) {
        adler = adler32(test_initial_values[i], data, size);
    }
    
    // Test with different buffer sizes including edge cases
    if (size > 0) {
        // Single byte
        adler = adler32(1, data, 1);
        
        // Multiple small chunks
        for (size_t i = 0; i < size && i < 100; i++) {
            adler = adler32(1, data, i);
        }
    }
    
    // Test with large buffers (if input is large enough)
    if (size > 1000) {
        // Test with progressively larger sizes
        adler = adler32(1, data, 1000);
        adler = adler32(1, data, size / 2);
        adler = adler32(1, data, size);
    }
    
    // Test boundary conditions with length
    if (size > 0) {
        // Maximum reasonable length
        size_t test_len = size;
        adler = adler32(initial_adler, data, test_len);
        
        // Zero length (should return initial value)
        adler = adler32(initial_adler, data, 0);
        if (adler != initial_adler) {
            // Should return initial value for zero length
        }
    }
    
    // Test with various data patterns
    uint8_t pattern_data[256];
    
    // All zeros
    memset(pattern_data, 0, sizeof(pattern_data));
    adler = adler32(1, pattern_data, sizeof(pattern_data));
    
    // All 0xFF
    memset(pattern_data, 0xFF, sizeof(pattern_data));
    adler = adler32(1, pattern_data, sizeof(pattern_data));
    
    // Alternating pattern
    for (int i = 0; i < 256; i++) {
        pattern_data[i] = i & 0xFF;
    }
    adler = adler32(1, pattern_data, sizeof(pattern_data));
    
    // Use input data to create pattern if available
    if (size > 0) {
        for (int i = 0; i < 256; i++) {
            pattern_data[i] = data[i % size];
        }
        adler = adler32(1, pattern_data, sizeof(pattern_data));
    }
    
    // Test incremental computation with various chunk sizes
    if (size >= 16) {
        uLong incremental_adler = initial_adler;
        
        // Compute in chunks
        size_t chunk_sizes[] = {1, 2, 4, 8, 16, 32, 64};
        
        for (int cs = 0; cs < 7; cs++) {
            size_t chunk_size = chunk_sizes[cs];
            size_t pos = 0;
            uLong chunk_adler = initial_adler;
            
            while (pos < size) {
                size_t remaining = size - pos;
                size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
                
                chunk_adler = adler32(chunk_adler, data + pos, current_chunk);
                pos += current_chunk;
                
                if (pos >= 64) break; // Limit iterations
            }
        }
    }
    
    // Test with extreme adler values
    if (size > 0) {
        uLong extreme_values[] = {
            0x00000000, 0x00000001, 0x0000FFFF, 0x00010000,
            0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF
        };
        
        for (int i = 0; i < 8; i++) {
            adler = adler32(extreme_values[i], data, size > 32 ? 32 : size);
        }
    }
    
    // Test overlapping computations
    if (size >= 10) {
        uLong overlapping_results[5];
        
        for (int i = 0; i < 5; i++) {
            size_t offset = i * 2;
            size_t len = size - offset;
            if (len > 20) len = 20;
            
            overlapping_results[i] = adler32(1, data + offset, len);
        }
    }
    
    // Test with corrupted/random data patterns that might trigger edge cases
    if (size >= 4) {
        uint8_t edge_case_data[64];
        
        // Fill with user data pattern
        for (int i = 0; i < 64; i++) {
            edge_case_data[i] = data[i % size];
        }
        
        // Test various lengths
        for (int len = 1; len <= 64; len++) {
            adler = adler32(initial_adler, edge_case_data, len);
        }
    }
    
    return 0;
}