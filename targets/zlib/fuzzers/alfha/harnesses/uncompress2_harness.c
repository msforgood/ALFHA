#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define FC_ZLIB_UNCOMPRESS2 0x26

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
    
    source_len = ((uLong*)data)[0];
    size_t data_offset = sizeof(uLong);
    
    if (size <= data_offset) {
        return 0;
    }
    
    // Limit source_len to reasonable values
    if (source_len > size - data_offset) {
        source_len = size - data_offset;
    }
    
    dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
    if (!dest_buffer) {
        return 0;
    }
    
    // Test with NULL pointers (should fail gracefully) 
    dest_len = MAX_DEST_SIZE;
    ret = uncompress2(NULL, &dest_len, data + data_offset, &source_len);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL dest
    }
    
    ret = uncompress2(dest_buffer, NULL, data + data_offset, &source_len);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL destLen
    }
    
    ret = uncompress2(dest_buffer, &dest_len, data + data_offset, NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL sourceLen
    }
    
    // Test with empty input
    uLong zero_len = 0;
    dest_len = MAX_DEST_SIZE;
    ret = uncompress2(dest_buffer, &dest_len, NULL, &zero_len);
    
    // Test with actual compressed data
    if (source_len > 0 && size > data_offset) {
        source_buffer = (Bytef*)malloc(source_len);
        if (source_buffer) {
            memcpy(source_buffer, data + data_offset, source_len);
            
            dest_len = MAX_DEST_SIZE;
            uLong test_source_len = source_len;
            ret = uncompress2(dest_buffer, &dest_len, source_buffer, &test_source_len);
            
            // Test with undersized destination buffer
            uLongf small_dest_len = 16;
            test_source_len = source_len;
            ret = uncompress2(dest_buffer, &small_dest_len, source_buffer, &test_source_len);
            
            // Test with partial source data
            uLong partial_source_len = source_len / 2;
            dest_len = MAX_DEST_SIZE;
            ret = uncompress2(dest_buffer, &dest_len, source_buffer, &partial_source_len);
            
            // Test with zero destination length
            uLongf zero_dest_len = 0;
            test_source_len = source_len;
            ret = uncompress2(dest_buffer, &zero_dest_len, source_buffer, &test_source_len);
            
            free(source_buffer);
        }
    }
    
    // Test with valid compressed data (create some first)
    if (size > data_offset + 1) {
        Bytef *test_data = (Bytef*)malloc(32);
        Bytef *compressed_data = (Bytef*)malloc(64);
        if (test_data && compressed_data) {
            // Fill test data with simple pattern
            for (int i = 0; i < 32; i++) {
                test_data[i] = i % 256;
            }
            
            uLongf compressed_len = 64;
            ret = compress(compressed_data, &compressed_len, test_data, 32);
            if (ret == Z_OK) {
                dest_len = MAX_DEST_SIZE;
                uLong comp_len = compressed_len;
                ret = uncompress2(dest_buffer, &dest_len, compressed_data, &comp_len);
            }
            
            free(test_data);
            free(compressed_data);
        }
    }
    
    // Test corrupted data patterns
    if (size > data_offset + 8) {
        Bytef corrupted[8];
        memcpy(corrupted, data + data_offset, 8);
        
        // Corrupt various positions
        for (int i = 0; i < 8; i++) {
            corrupted[i] ^= 0xFF;
            dest_len = MAX_DEST_SIZE;
            uLong corr_len = 8;
            ret = uncompress2(dest_buffer, &dest_len, corrupted, &corr_len);
            corrupted[i] ^= 0xFF; // restore
        }
    }
    
    free(dest_buffer);
    return 0;
}