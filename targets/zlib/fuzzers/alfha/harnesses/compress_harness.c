#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_COMPRESS 0x07

#define MAX_SOURCE_SIZE 4096
#define MAX_DEST_SIZE 8192

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(uLong)) {
        return 0;
    }
    
    Bytef *source_buffer = NULL;
    Bytef *dest_buffer = NULL;
    uLongf dest_len;
    uLong source_len;
    int ret;
    
    // Test with NULL pointers (should fail gracefully)
    dest_len = MAX_DEST_SIZE;
    ret = compress(NULL, &dest_len, data, size);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL dest
    }
    
    dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
    if (!dest_buffer) {
        return 0;
    }
    
    ret = compress(dest_buffer, NULL, data, size);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL destLen
    }
    
    dest_len = MAX_DEST_SIZE;
    ret = compress(dest_buffer, &dest_len, NULL, size);
    if (size > 0 && ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL source with non-zero size
    }
    
    // Test with valid parameters
    source_len = size > MAX_SOURCE_SIZE ? MAX_SOURCE_SIZE : size;
    if (source_len > 0) {
        source_buffer = (Bytef*)malloc(source_len);
        if (source_buffer) {
            memcpy(source_buffer, data, source_len);
            
            dest_len = compressBound(source_len);
            if (dest_len > MAX_DEST_SIZE) {
                dest_len = MAX_DEST_SIZE;
            }
            
            ret = compress(dest_buffer, &dest_len, source_buffer, source_len);
            
            free(source_buffer);
        }
    }
    
    // Test with zero-length input
    dest_len = MAX_DEST_SIZE;
    ret = compress(dest_buffer, &dest_len, data, 0);
    
    // Test with insufficient destination buffer
    if (size > 0) {
        source_len = size > 100 ? 100 : size;
        dest_len = 1; // Intentionally too small
        ret = compress(dest_buffer, &dest_len, data, source_len);
        // Should return Z_BUF_ERROR
    }
    
    // Test with exact destination buffer size
    if (size > 0) {
        source_len = size > 64 ? 64 : size;
        dest_len = compressBound(source_len);
        
        if (dest_len <= MAX_DEST_SIZE) {
            ret = compress(dest_buffer, &dest_len, data, source_len);
            // Should succeed
        }
    }
    
    // Test with oversized destination buffer
    if (size > 0) {
        source_len = size > 32 ? 32 : size;
        dest_len = MAX_DEST_SIZE;
        ret = compress(dest_buffer, &dest_len, data, source_len);
    }
    
    free(dest_buffer);
    
    // Test various input patterns and sizes
    for (uLong test_size = 0; test_size <= size && test_size <= 256; test_size += (test_size < 16) ? 1 : 16) {
        dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
        if (!dest_buffer) continue;
        
        dest_len = MAX_DEST_SIZE;
        ret = compress(dest_buffer, &dest_len, data, test_size);
        
        free(dest_buffer);
    }
    
    // Test with different data patterns
    if (size >= 16) {
        uint8_t pattern_buffers[4][256];
        
        // Pattern 1: All zeros
        memset(pattern_buffers[0], 0, 256);
        
        // Pattern 2: All 0xFF
        memset(pattern_buffers[1], 0xFF, 256);
        
        // Pattern 3: Repeating pattern
        for (int i = 0; i < 256; i++) {
            pattern_buffers[2][i] = i % 16;
        }
        
        // Pattern 4: From input data
        size_t copy_size = size > 256 ? 256 : size;
        memcpy(pattern_buffers[3], data, copy_size);
        if (copy_size < 256) {
            memset(pattern_buffers[3] + copy_size, 0, 256 - copy_size);
        }
        
        for (int pattern = 0; pattern < 4; pattern++) {
            dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
            if (!dest_buffer) continue;
            
            dest_len = MAX_DEST_SIZE;
            ret = compress(dest_buffer, &dest_len, pattern_buffers[pattern], 256);
            
            free(dest_buffer);
        }
    }
    
    // Test maximum size inputs (within limits)
    if (size >= 4) {
        uLong max_test_size = MAX_SOURCE_SIZE;
        source_buffer = (Bytef*)malloc(max_test_size);
        dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
        
        if (source_buffer && dest_buffer) {
            // Fill with pattern from input data
            for (uLong i = 0; i < max_test_size; i++) {
                source_buffer[i] = data[i % size];
            }
            
            dest_len = MAX_DEST_SIZE;
            ret = compress(dest_buffer, &dest_len, source_buffer, max_test_size);
        }
        
        free(source_buffer);
        free(dest_buffer);
    }
    
    // Test boundary conditions
    if (size > 0) {
        dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
        if (dest_buffer) {
            // Test with destLen exactly at boundary
            uLong boundary_size = size > 16 ? 16 : size;
            dest_len = compressBound(boundary_size);
            
            if (dest_len <= MAX_DEST_SIZE) {
                ret = compress(dest_buffer, &dest_len, data, boundary_size);
            }
            
            // Test with destLen one less than needed
            if (dest_len > 1 && dest_len <= MAX_DEST_SIZE) {
                dest_len--;
                ret = compress(dest_buffer, &dest_len, data, boundary_size);
                // May return Z_BUF_ERROR
            }
            
            free(dest_buffer);
        }
    }
    
    // Stress test with rapid allocations
    for (int stress = 0; stress < 10 && stress < (int)size; stress++) {
        dest_buffer = (Bytef*)malloc(512);
        if (!dest_buffer) break;
        
        dest_len = 512;
        uLong stress_size = (stress + 1) * 2;
        if (stress_size > size) stress_size = size;
        
        ret = compress(dest_buffer, &dest_len, data, stress_size);
        
        free(dest_buffer);
    }
    
    return 0;
}