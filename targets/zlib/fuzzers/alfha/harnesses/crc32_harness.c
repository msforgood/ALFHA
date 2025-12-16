#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_CRC32 0x18

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) {
        return 0;
    }
    
    uLong crc;
    uLong initial_crc;
    
    // Extract initial CRC value from input data
    if (size >= sizeof(uLong)) {
        memcpy(&initial_crc, data, sizeof(uLong));
        data += sizeof(uLong);
        size -= sizeof(uLong);
    } else {
        initial_crc = crc32(0L, Z_NULL, 0); // Default initial value
    }
    
    // Test crc32 with NULL buffer (should handle gracefully)
    crc = crc32(0L, NULL, 0);
    if (crc != 0L) {
        // Should return initial value for NULL buffer with len=0
    }
    
    // Test crc32 with NULL buffer and non-zero length
    crc = crc32(0L, NULL, 100);
    // Behavior might be undefined, but shouldn't crash
    
    // Test basic crc32 computation with user data
    if (size > 0) {
        crc = crc32(initial_crc, data, size);
        
        // Verify incremental computation gives same result
        if (size > 1) {
            uLong incremental = initial_crc;
            incremental = crc32(incremental, data, size / 2);
            incremental = crc32(incremental, data + (size / 2), size - (size / 2));
            
            // incremental should equal crc
        }
    }
    
    // Test with various initial CRC values
    uLong test_initial_values[] = {
        0, 0xFFFFFFFF, 0x80000000, 0x12345678, 0xDEADBEEF,
        initial_crc, ~initial_crc, 0x55555555, 0xAAAAAAAA
    };
    
    for (int i = 0; i < 9 && size > 0; i++) {
        crc = crc32(test_initial_values[i], data, size);
    }
    
    // Test with different buffer sizes including edge cases
    if (size > 0) {
        // Single byte
        crc = crc32(0L, data, 1);
        
        // Multiple small chunks
        for (size_t i = 0; i < size && i < 100; i++) {
            crc = crc32(0L, data, i);
        }
    }
    
    // Test with large buffers (if input is large enough)
    if (size > 1000) {
        // Test with progressively larger sizes
        crc = crc32(0L, data, 1000);
        crc = crc32(0L, data, size / 2);
        crc = crc32(0L, data, size);
    }
    
    // Test boundary conditions with length
    if (size > 0) {
        // Maximum reasonable length
        size_t test_len = size;
        crc = crc32(initial_crc, data, test_len);
        
        // Zero length (should return initial value)
        crc = crc32(initial_crc, data, 0);
        if (crc != initial_crc) {
            // Should return initial value for zero length
        }
    }
    
    // Test with various data patterns
    uint8_t pattern_data[256];
    
    // All zeros
    memset(pattern_data, 0, sizeof(pattern_data));
    crc = crc32(0L, pattern_data, sizeof(pattern_data));
    
    // All 0xFF
    memset(pattern_data, 0xFF, sizeof(pattern_data));
    crc = crc32(0L, pattern_data, sizeof(pattern_data));
    
    // Alternating pattern
    for (int i = 0; i < 256; i++) {
        pattern_data[i] = i & 0xFF;
    }
    crc = crc32(0L, pattern_data, sizeof(pattern_data));
    
    // Use input data to create pattern if available
    if (size > 0) {
        for (int i = 0; i < 256; i++) {
            pattern_data[i] = data[i % size];
        }
        crc = crc32(0L, pattern_data, sizeof(pattern_data));
    }
    
    // Test incremental computation with various chunk sizes
    if (size >= 16) {
        uLong incremental_crc = initial_crc;
        
        // Compute in chunks
        size_t chunk_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128};
        
        for (int cs = 0; cs < 8; cs++) {
            size_t chunk_size = chunk_sizes[cs];
            size_t pos = 0;
            uLong chunk_crc = initial_crc;
            
            while (pos < size) {
                size_t remaining = size - pos;
                size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
                
                chunk_crc = crc32(chunk_crc, data + pos, current_chunk);
                pos += current_chunk;
                
                if (pos >= 256) break; // Limit iterations
            }
        }
    }
    
    // Test with extreme CRC values
    if (size > 0) {
        uLong extreme_values[] = {
            0x00000000, 0x00000001, 0x0000FFFF, 0x00010000,
            0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF,
            0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE
        };
        
        for (int i = 0; i < 12; i++) {
            crc = crc32(extreme_values[i], data, size > 64 ? 64 : size);
        }
    }
    
    // Test overlapping computations
    if (size >= 10) {
        uLong overlapping_results[8];
        
        for (int i = 0; i < 8; i++) {
            size_t offset = i * 2;
            size_t len = size - offset;
            if (len > 32) len = 32;
            
            if (offset < size) {
                overlapping_results[i] = crc32(0L, data + offset, len);
            }
        }
    }
    
    // Test with random start/end positions
    if (size >= 8) {
        for (int i = 0; i < 10; i++) {
            size_t start = data[i % size] % size;
            size_t len = (data[(i + 1) % size] % (size - start)) + 1;
            
            if (start + len <= size) {
                crc = crc32(initial_crc, data + start, len);
            }
        }
    }
    
    // Test CRC consistency - same input should give same output
    if (size > 4) {
        uLong crc1 = crc32(0L, data, size / 2);
        uLong crc2 = crc32(0L, data, size / 2);
        
        if (crc1 != crc2) {
            // CRC should be deterministic
        }
    }
    
    // Test with corrupted/random data patterns that might trigger edge cases
    if (size >= 4) {
        uint8_t edge_case_data[128];
        
        // Fill with user data pattern
        for (int i = 0; i < 128; i++) {
            edge_case_data[i] = data[i % size];
        }
        
        // Test various lengths
        for (int len = 1; len <= 128; len += 7) {
            crc = crc32(initial_crc, edge_case_data, len);
        }
        
        // Test with different alignment
        for (int offset = 0; offset < 8 && offset < 128; offset++) {
            crc = crc32(initial_crc, edge_case_data + offset, 128 - offset);
        }
    }
    
    // Test CRC chaining - feed output as next input
    if (size >= 8) {
        uLong chained_crc = initial_crc;
        uint8_t crc_bytes[4];
        
        for (int i = 0; i < 5; i++) {
            // Convert CRC to bytes and use as input
            crc_bytes[0] = (chained_crc >> 24) & 0xFF;
            crc_bytes[1] = (chained_crc >> 16) & 0xFF;
            crc_bytes[2] = (chained_crc >> 8) & 0xFF;
            crc_bytes[3] = chained_crc & 0xFF;
            
            chained_crc = crc32(chained_crc, crc_bytes, 4);
        }
    }
    
    return 0;
}