#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec  
#define FC_ZLIB_UNCOMPRESS 0x08

#define MAX_SOURCE_SIZE 4096
#define MAX_DEST_SIZE 8192

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(uLong)) {
        return 0;
    }
    
    Bytef *dest_buffer = NULL;
    uLongf dest_len;
    uLong source_len;
    int ret;
    
    // Test with NULL pointers (should fail gracefully)
    dest_len = MAX_DEST_SIZE;
    ret = uncompress(NULL, &dest_len, data, size);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL dest
    }
    
    dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
    if (!dest_buffer) {
        return 0;
    }
    
    ret = uncompress(dest_buffer, NULL, data, size);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL destLen
    }
    
    dest_len = MAX_DEST_SIZE;
    ret = uncompress(dest_buffer, &dest_len, NULL, size);
    if (size > 0 && ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL source with non-zero size
    }
    
    // Test with raw input data as compressed data (likely corrupted)
    source_len = size > MAX_SOURCE_SIZE ? MAX_SOURCE_SIZE : size;
    dest_len = MAX_DEST_SIZE;
    ret = uncompress(dest_buffer, &dest_len, data, source_len);
    // Will likely return Z_DATA_ERROR for random input
    
    // Test with zero-length input
    dest_len = MAX_DEST_SIZE;
    ret = uncompress(dest_buffer, &dest_len, data, 0);
    
    free(dest_buffer);
    
    // Create valid compressed data and test decompression
    if (size >= 4) {
        Bytef *compressed_buffer = (Bytef*)malloc(MAX_SOURCE_SIZE);
        if (!compressed_buffer) {
            return 0;
        }
        
        // Create test data to compress
        uLong orig_size = size > 1024 ? 1024 : size;
        Bytef *orig_data = (Bytef*)malloc(orig_size);
        if (!orig_data) {
            free(compressed_buffer);
            return 0;
        }
        
        memcpy(orig_data, data, orig_size);
        
        // Compress the data
        uLongf compressed_size = MAX_SOURCE_SIZE;
        ret = compress(compressed_buffer, &compressed_size, orig_data, orig_size);
        
        if (ret == Z_OK) {
            // Now test decompression with various destination buffer sizes
            
            // Test with correct destination size
            dest_buffer = (Bytef*)malloc(orig_size);
            if (dest_buffer) {
                dest_len = orig_size;
                ret = uncompress(dest_buffer, &dest_len, compressed_buffer, compressed_size);
                free(dest_buffer);
            }
            
            // Test with insufficient destination buffer
            if (orig_size > 1) {
                dest_buffer = (Bytef*)malloc(orig_size - 1);
                if (dest_buffer) {
                    dest_len = orig_size - 1;
                    ret = uncompress(dest_buffer, &dest_len, compressed_buffer, compressed_size);
                    // Should return Z_BUF_ERROR
                    free(dest_buffer);
                }
            }
            
            // Test with oversized destination buffer
            dest_buffer = (Bytef*)malloc(orig_size * 2);
            if (dest_buffer) {
                dest_len = orig_size * 2;
                ret = uncompress(dest_buffer, &dest_len, compressed_buffer, compressed_size);
                free(dest_buffer);
            }
            
            // Test with zero destination size
            dest_buffer = (Bytef*)malloc(1);
            if (dest_buffer) {
                dest_len = 0;
                ret = uncompress(dest_buffer, &dest_len, compressed_buffer, compressed_size);
                // Should return Z_BUF_ERROR
                free(dest_buffer);
            }
        }
        
        free(orig_data);
        free(compressed_buffer);
    }
    
    // Test with truncated compressed data
    if (size >= 10) {
        // Create some compressed data first
        uLong test_size = 64;
        Bytef *test_data = (Bytef*)malloc(test_size);
        Bytef *comp_buffer = (Bytef*)malloc(test_size * 2);
        
        if (test_data && comp_buffer) {
            // Fill test data with pattern from input
            for (uLong i = 0; i < test_size; i++) {
                test_data[i] = data[i % size];
            }
            
            uLongf comp_size = test_size * 2;
            ret = compress(comp_buffer, &comp_size, test_data, test_size);
            
            if (ret == Z_OK && comp_size > 1) {
                // Test with truncated compressed data
                for (uLong truncate = 1; truncate < comp_size && truncate < 16; truncate++) {
                    dest_buffer = (Bytef*)malloc(test_size);
                    if (dest_buffer) {
                        dest_len = test_size;
                        ret = uncompress(dest_buffer, &dest_len, comp_buffer, comp_size - truncate);
                        // Should return Z_DATA_ERROR or Z_BUF_ERROR
                        free(dest_buffer);
                    }
                }
            }
        }
        
        free(test_data);
        free(comp_buffer);
    }
    
    // Test with corrupted compressed data
    if (size >= 8) {
        // Create valid compressed data then corrupt it
        uLong pattern_size = 32;
        Bytef pattern_data[32];
        Bytef *comp_buffer = (Bytef*)malloc(128);
        
        if (comp_buffer) {
            // Create pattern from input data
            for (int i = 0; i < 32; i++) {
                pattern_data[i] = data[i % size];
            }
            
            uLongf comp_size = 128;
            ret = compress(comp_buffer, &comp_size, pattern_data, pattern_size);
            
            if (ret == Z_OK) {
                // Corrupt the compressed data
                for (int corrupt_pos = 0; corrupt_pos < (int)comp_size && corrupt_pos < 8; corrupt_pos++) {
                    Bytef saved_byte = comp_buffer[corrupt_pos];
                    comp_buffer[corrupt_pos] ^= 0xFF; // Flip all bits
                    
                    dest_buffer = (Bytef*)malloc(pattern_size * 2);
                    if (dest_buffer) {
                        dest_len = pattern_size * 2;
                        ret = uncompress(dest_buffer, &dest_len, comp_buffer, comp_size);
                        // Should return Z_DATA_ERROR
                        free(dest_buffer);
                    }
                    
                    comp_buffer[corrupt_pos] = saved_byte; // Restore
                }
            }
            
            free(comp_buffer);
        }
    }
    
    // Test various input sizes
    for (uLong test_size = 0; test_size <= size && test_size <= 128; test_size += (test_size < 16) ? 1 : 16) {
        dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
        if (!dest_buffer) continue;
        
        dest_len = MAX_DEST_SIZE;
        ret = uncompress(dest_buffer, &dest_len, data, test_size);
        
        free(dest_buffer);
    }
    
    // Test with different data patterns as compressed input
    if (size >= 16) {
        uint8_t pattern_buffers[3][64];
        
        // Pattern 1: All zeros (invalid compressed data)
        memset(pattern_buffers[0], 0, 64);
        
        // Pattern 2: All 0xFF (invalid compressed data)  
        memset(pattern_buffers[1], 0xFF, 64);
        
        // Pattern 3: Sequential pattern
        for (int i = 0; i < 64; i++) {
            pattern_buffers[2][i] = i;
        }
        
        for (int pattern = 0; pattern < 3; pattern++) {
            dest_buffer = (Bytef*)malloc(512);
            if (!dest_buffer) continue;
            
            dest_len = 512;
            ret = uncompress(dest_buffer, &dest_len, pattern_buffers[pattern], 64);
            // Will likely return Z_DATA_ERROR
            
            free(dest_buffer);
        }
    }
    
    // Test boundary conditions with valid compressed data
    if (size >= 1) {
        // Create minimal valid data
        Bytef minimal_data = data[0];
        Bytef *comp_buffer = (Bytef*)malloc(16);
        
        if (comp_buffer) {
            uLongf comp_size = 16;
            ret = compress(comp_buffer, &comp_size, &minimal_data, 1);
            
            if (ret == Z_OK) {
                // Test exact buffer size
                dest_buffer = (Bytef*)malloc(1);
                if (dest_buffer) {
                    dest_len = 1;
                    ret = uncompress(dest_buffer, &dest_len, comp_buffer, comp_size);
                    free(dest_buffer);
                }
                
                // Test with partial compressed input
                if (comp_size > 1) {
                    dest_buffer = (Bytef*)malloc(2);
                    if (dest_buffer) {
                        dest_len = 2;
                        ret = uncompress(dest_buffer, &dest_len, comp_buffer, comp_size - 1);
                        free(dest_buffer);
                    }
                }
            }
            
            free(comp_buffer);
        }
    }
    
    return 0;
}