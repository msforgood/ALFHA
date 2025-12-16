#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATE 0x02

#define MAX_BUFFER_SIZE 4096

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t input_buffer[MAX_BUFFER_SIZE];
    uint8_t output_buffer[MAX_BUFFER_SIZE];
    int flush_mode;
    size_t input_size;
    
    // Extract flush mode from first byte
    flush_mode = data[0];
    
    // Test with NULL pointer (should fail gracefully)
    ret = deflate(NULL, flush_mode);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior, but continue
    }
    
    // Initialize stream for testing
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        return 0; // Cannot proceed without initialized stream
    }
    
    // Prepare input data
    input_size = (size - 1) > MAX_BUFFER_SIZE ? MAX_BUFFER_SIZE : (size - 1);
    if (input_size > 0) {
        memcpy(input_buffer, data + 1, input_size);
    }
    
    // Test various flush modes
    int valid_flush_modes[] = {
        Z_NO_FLUSH, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, 
        Z_FULL_FLUSH, Z_FINISH, Z_BLOCK
    };
    
    // Test with invalid flush mode first
    strm.next_in = input_buffer;
    strm.avail_in = input_size;
    strm.next_out = output_buffer;
    strm.avail_out = MAX_BUFFER_SIZE;
    
    ret = deflate(&strm, flush_mode); // May be invalid
    
    // Reset for valid testing
    deflateEnd(&strm);
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        return 0;
    }
    
    // Test with different valid flush modes
    for (int i = 0; i < sizeof(valid_flush_modes)/sizeof(valid_flush_modes[0]); i++) {
        strm.next_in = input_buffer;
        strm.avail_in = input_size;
        strm.next_out = output_buffer;
        strm.avail_out = MAX_BUFFER_SIZE;
        
        ret = deflate(&strm, valid_flush_modes[i]);
        
        // Continue processing if not finished
        while (ret == Z_OK && strm.avail_out == 0) {
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            ret = deflate(&strm, valid_flush_modes[i]);
        }
        
        if (ret == Z_STREAM_END) {
            break; // Finished compression
        }
    }
    
    // Test incremental feeding
    deflateEnd(&strm);
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        // Feed data in small chunks
        size_t chunk_size = 1;
        size_t offset = 1; // Skip flush mode byte
        
        while (offset < size && chunk_size <= size - offset) {
            size_t current_chunk = chunk_size;
            if (current_chunk > MAX_BUFFER_SIZE) {
                current_chunk = MAX_BUFFER_SIZE;
            }
            if (current_chunk > size - offset) {
                current_chunk = size - offset;
            }
            
            memcpy(input_buffer, data + offset, current_chunk);
            
            strm.next_in = input_buffer;
            strm.avail_in = current_chunk;
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            
            ret = deflate(&strm, Z_NO_FLUSH);
            
            offset += current_chunk;
            chunk_size *= 2; // Exponential chunk size increase
            
            if (ret != Z_OK) {
                break;
            }
        }
        
        // Final flush
        strm.next_in = Z_NULL;
        strm.avail_in = 0;
        strm.next_out = output_buffer;
        strm.avail_out = MAX_BUFFER_SIZE;
        
        do {
            ret = deflate(&strm, Z_FINISH);
            if (strm.avail_out == 0) {
                strm.next_out = output_buffer;
                strm.avail_out = MAX_BUFFER_SIZE;
            }
        } while (ret == Z_OK);
        
        deflateEnd(&strm);
    }
    
    // Test edge cases
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        // Test with zero avail_out (should return Z_BUF_ERROR)
        strm.next_in = input_buffer;
        strm.avail_in = 1;
        strm.next_out = output_buffer;
        strm.avail_out = 0;
        
        ret = deflate(&strm, Z_NO_FLUSH);
        
        // Test with NULL next_out but non-zero avail_out
        strm.next_out = Z_NULL;
        strm.avail_out = 100;
        ret = deflate(&strm, Z_NO_FLUSH);
        
        deflateEnd(&strm);
    }
    
    return 0;
}