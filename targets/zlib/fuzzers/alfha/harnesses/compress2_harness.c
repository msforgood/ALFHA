#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define FC_ZLIB_COMPRESS2 0x25

#define MAX_SOURCE_SIZE 4096
#define MAX_DEST_SIZE 8192

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(int)) {
        return 0;
    }
    
    Bytef *source_buffer = NULL;
    Bytef *dest_buffer = NULL;
    uLongf dest_len;
    uLong source_len;
    int level;
    int ret;
    
    level = ((int*)data)[0];
    size_t data_offset = sizeof(int);
    
    if (size <= data_offset) {
        return 0;
    }
    
    dest_buffer = (Bytef*)malloc(MAX_DEST_SIZE);
    if (!dest_buffer) {
        return 0;
    }
    
    // Test with NULL pointers (should fail gracefully)
    dest_len = MAX_DEST_SIZE;
    ret = compress2(NULL, &dest_len, data + data_offset, size - data_offset, level);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL dest
    }
    
    ret = compress2(dest_buffer, NULL, data + data_offset, size - data_offset, level);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL destLen
    }
    
    // Test with empty input
    dest_len = MAX_DEST_SIZE;
    ret = compress2(dest_buffer, &dest_len, NULL, 0, level);
    
    // Test with actual data
    source_len = (size - data_offset) > MAX_SOURCE_SIZE ? MAX_SOURCE_SIZE : (size - data_offset);
    if (source_len > 0) {
        source_buffer = (Bytef*)malloc(source_len);
        if (source_buffer) {
            memcpy(source_buffer, data + data_offset, source_len);
            
            dest_len = MAX_DEST_SIZE;
            ret = compress2(dest_buffer, &dest_len, source_buffer, source_len, level);
            
            // Test with undersized destination buffer
            uLongf small_dest_len = source_len / 4;
            if (small_dest_len > 0) {
                ret = compress2(dest_buffer, &small_dest_len, source_buffer, source_len, level);
            }
            
            // Test with exact compressBound size
            uLongf bound_size = compressBound(source_len);
            if (bound_size <= MAX_DEST_SIZE) {
                dest_len = bound_size;
                ret = compress2(dest_buffer, &dest_len, source_buffer, source_len, level);
            }
            
            free(source_buffer);
        }
    }
    
    // Test various compression levels
    int test_levels[] = {Z_NO_COMPRESSION, Z_BEST_SPEED, Z_BEST_COMPRESSION, 
                        Z_DEFAULT_COMPRESSION, -2, 10, 100};
    for (int i = 0; i < sizeof(test_levels)/sizeof(test_levels[0]); i++) {
        dest_len = MAX_DEST_SIZE;
        ret = compress2(dest_buffer, &dest_len, data + data_offset, 
                       source_len, test_levels[i]);
    }
    
    free(dest_buffer);
    return 0;
}