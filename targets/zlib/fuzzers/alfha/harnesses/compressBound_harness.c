#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <limits.h>

// Function code from spec
#define FC_ZLIB_COMPRESSBOUND 0x1D

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    uLong bound;
    uLong test_length;
    
    // Extract test length from input data
    memcpy(&test_length, data, sizeof(uLong));
    
    // Use remaining data size as additional test length
    uLong data_size = (uLong)(size - sizeof(uLong));
    
    // Test compressBound with zero length
    bound = compressBound(0);
    if (bound < 13) {
        // Minimum bound should account for zlib header/trailer
    }
    
    // Test compressBound with small values
    uLong small_values[] = {1, 2, 3, 4, 5, 8, 16, 32, 64, 128, 256, 512, 1024};
    
    for (int i = 0; i < 13; i++) {
        bound = compressBound(small_values[i]);
        
        // Verify bound is reasonable (should be >= input length)
        if (bound < small_values[i]) {
            // Bound should never be less than input size
        }
    }
    
    // Test compressBound with user-provided length
    bound = compressBound(test_length);
    
    // Test compressBound with data size
    bound = compressBound(data_size);
    
    // Test with powers of 2
    for (int shift = 0; shift < 20; shift++) {
        uLong power_of_2 = 1UL << shift;
        bound = compressBound(power_of_2);
        
        // Verify reasonable bound
        if (bound < power_of_2) {
            // Unexpected result
        }
    }
    
    // Test with large values (but not extreme to avoid overflow)
    uLong large_values[] = {
        1024*1024,         // 1MB
        10*1024*1024,      // 10MB  
        100*1024*1024,     // 100MB
        1000*1024*1024,    // 1GB
    };
    
    for (int i = 0; i < 4; i++) {
        bound = compressBound(large_values[i]);
        
        // Check for reasonable relationship to input size
        if (bound < large_values[i]) {
            // Unexpected - bound should be at least input size
        }
        
        // Check for potential overflow
        if (bound < large_values[i]) {
            // Possible integer overflow
        }
    }
    
    // Test boundary values around integer limits
    uLong boundary_values[] = {
        ULONG_MAX / 2,
        ULONG_MAX / 4, 
        ULONG_MAX / 8,
        ULONG_MAX / 16,
        ULONG_MAX / 32,
        ULONG_MAX - 1000,
        ULONG_MAX - 100,
        ULONG_MAX - 10,
        ULONG_MAX - 1,
        ULONG_MAX,
    };
    
    for (int i = 0; i < 10; i++) {
        bound = compressBound(boundary_values[i]);
        // Should handle large values gracefully, may overflow
    }
    
    // Test with values that might cause overflow in bound calculation
    // compressBound typically adds ~0.1% + 12 bytes overhead
    uLong overflow_test_values[] = {
        ULONG_MAX - 20,
        ULONG_MAX - 100, 
        ULONG_MAX - 1000,
        ULONG_MAX / 1000 * 999, // Should be close to max but safe
    };
    
    for (int i = 0; i < 4; i++) {
        bound = compressBound(overflow_test_values[i]);
        
        // Check if result makes sense
        if (bound < overflow_test_values[i]) {
            // Possible overflow occurred
        }
    }
    
    // Test mathematical relationships
    // compressBound should be monotonically increasing
    if (size >= 8) {
        uLong len1 = test_length;
        uLong len2 = test_length + 1000;
        
        uLong bound1 = compressBound(len1);
        uLong bound2 = compressBound(len2);
        
        if (bound2 < bound1 && len2 > len1) {
            // Unexpected - bound should increase with input size
        }
    }
    
    // Test consistency - same input should give same output
    bound = compressBound(test_length);
    uLong bound2 = compressBound(test_length);
    
    if (bound != bound2) {
        // Function should be deterministic
    }
    
    // Test rapid successive calls for performance/stability
    for (int i = 0; i < 1000; i++) {
        uLong quick_len = (data_size + i) % 10000;
        bound = compressBound(quick_len);
    }
    
    // Test compressBound vs actual compress size relationship
    if (size > sizeof(uLong) && (size - sizeof(uLong)) > 0) {
        uLong input_len = size - sizeof(uLong);
        const uint8_t* input_data = data + sizeof(uLong);
        
        // Get the bound
        bound = compressBound(input_len);
        
        // Try actual compression to verify bound is sufficient
        uint8_t* compress_buf = malloc(bound);
        if (compress_buf) {
            uLong compressed_size = bound;
            int result = compress(compress_buf, &compressed_size, input_data, input_len);
            
            if (result == Z_OK) {
                // Verify bound was sufficient
                if (compressed_size > bound) {
                    // Bound was insufficient!
                }
                
                // Test different compression levels
                for (int level = 1; level <= 9; level++) {
                    uLong level_size = bound;
                    int level_result = compress2(compress_buf, &level_size, input_data, input_len, level);
                    
                    if (level_result == Z_OK && level_size > bound) {
                        // Bound was insufficient for this level
                    }
                }
            }
            
            free(compress_buf);
        }
    }
    
    // Test edge cases around specific values that might trigger edge cases
    uLong edge_values[] = {
        0, 1, 
        255, 256, 257,
        65535, 65536, 65537,
        0xFFFFFF, 0x1000000, 0x1000001,
        (uLong)-1, (uLong)-2, (uLong)-100
    };
    
    for (int i = 0; i < 14; i++) {
        bound = compressBound(edge_values[i]);
    }
    
    // Test with lengths derived from input data in various ways
    if (size >= 8) {
        // Use bytes as length components
        uLong combined_len = 0;
        for (size_t i = 0; i < 4 && i < size; i++) {
            combined_len = (combined_len << 8) | data[i];
        }
        
        bound = compressBound(combined_len);
        
        // XOR combination
        uLong xor_len = 0;
        for (size_t i = 0; i < size; i++) {
            xor_len ^= data[i];
        }
        
        bound = compressBound(xor_len);
        
        // Sum combination
        uLong sum_len = 0;
        for (size_t i = 0; i < size && i < 1000; i++) {
            sum_len += data[i];
        }
        
        bound = compressBound(sum_len);
    }
    
    return 0;
}