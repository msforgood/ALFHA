#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATE 0x05

#define MAX_BUFFER_SIZE 4096

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) {
        return 0;
    }
    
    z_stream strm;
    int ret;
    uint8_t output_buffer[MAX_BUFFER_SIZE];
    int flush_mode;
    
    // Extract flush mode from first byte
    flush_mode = data[0];
    
    // Test with NULL pointer (should fail gracefully)
    ret = inflate(NULL, flush_mode);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior, but continue
    }
    
    // Initialize stream for testing
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return 0; // Cannot proceed without initialized stream
    }
    
    // Test various flush modes
    int valid_flush_modes[] = {
        Z_NO_FLUSH, Z_SYNC_FLUSH, Z_FINISH, Z_BLOCK, Z_TREES
    };
    
    // Test with input data as compressed stream
    size_t input_size = size - 1;
    if (input_size > 0) {
        strm.next_in = (Bytef*)(data + 1);
        strm.avail_in = input_size;
        strm.next_out = output_buffer;
        strm.avail_out = MAX_BUFFER_SIZE;
        
        // Test with original flush mode (may be invalid)
        ret = inflate(&strm, flush_mode);
        
        // Continue if not finished
        while (ret == Z_OK && strm.avail_out == 0) {
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            ret = inflate(&strm, flush_mode);
        }
    }
    
    inflateEnd(&strm);
    
    // Test with valid flush modes
    for (int i = 0; i < sizeof(valid_flush_modes)/sizeof(valid_flush_modes[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        if (ret != Z_OK) continue;
        
        strm.next_in = (Bytef*)(data + 1);
        strm.avail_in = input_size;
        strm.next_out = output_buffer;
        strm.avail_out = MAX_BUFFER_SIZE;
        
        ret = inflate(&strm, valid_flush_modes[i]);
        
        // Continue processing if needed
        while (ret == Z_OK && strm.avail_out == 0) {
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            ret = inflate(&strm, valid_flush_modes[i]);
        }
        
        inflateEnd(&strm);
    }
    
    // Test incremental feeding with small chunks
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        size_t chunk_size = 1;
        size_t offset = 1; // Skip flush mode byte
        
        while (offset < size && chunk_size <= size - offset) {
            size_t current_chunk = chunk_size;
            if (current_chunk > size - offset) {
                current_chunk = size - offset;
            }
            
            strm.next_in = (Bytef*)(data + offset);
            strm.avail_in = current_chunk;
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            
            ret = inflate(&strm, Z_NO_FLUSH);
            
            offset += current_chunk;
            chunk_size = (chunk_size < 16) ? chunk_size + 1 : chunk_size * 2;
            
            if (ret == Z_STREAM_END || ret < 0) {
                break;
            }
        }
        
        inflateEnd(&strm);
    }
    
    // Test with different window bits (raw deflate, zlib, gzip)
    int window_bits[] = { 15, -15, 15 + 16, 15 + 32 };
    for (int i = 0; i < sizeof(window_bits)/sizeof(window_bits[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit2(&strm, window_bits[i]);
        if (ret == Z_OK) {
            strm.next_in = (Bytef*)(data + 1);
            strm.avail_in = input_size;
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            
            ret = inflate(&strm, Z_NO_FLUSH);
            
            // Handle partial processing
            while (ret == Z_OK && strm.avail_in > 0 && strm.avail_out > 0) {
                ret = inflate(&strm, Z_NO_FLUSH);
            }
            
            inflateEnd(&strm);
        }
    }
    
    // Test edge cases
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        // Test with zero input
        strm.next_in = (Bytef*)(data + 1);
        strm.avail_in = 0;
        strm.next_out = output_buffer;
        strm.avail_out = MAX_BUFFER_SIZE;
        
        ret = inflate(&strm, Z_NO_FLUSH);
        
        // Test with NULL next_out but non-zero avail_out
        strm.avail_in = input_size;
        strm.next_out = Z_NULL;
        strm.avail_out = 100;
        ret = inflate(&strm, Z_NO_FLUSH);
        
        // Test with zero avail_out
        strm.next_out = output_buffer;
        strm.avail_out = 0;
        ret = inflate(&strm, Z_NO_FLUSH);
        
        inflateEnd(&strm);
    }
    
    // Test with corrupted compressed data
    if (size > 10) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            // Use raw input data as corrupted compressed data
            strm.next_in = (Bytef*)data;
            strm.avail_in = size;
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            
            ret = inflate(&strm, Z_NO_FLUSH);
            // Should handle corrupted data gracefully
            
            inflateEnd(&strm);
        }
    }
    
    // Create some valid compressed data and test inflation
    if (size > 4) {
        z_stream comp_strm;
        uint8_t compressed_buffer[MAX_BUFFER_SIZE];
        
        memset(&comp_strm, 0, sizeof(z_stream));
        ret = deflateInit(&comp_strm, Z_DEFAULT_COMPRESSION);
        if (ret == Z_OK) {
            // Compress part of the input data
            size_t comp_input_size = (size - 1) > 256 ? 256 : (size - 1);
            
            comp_strm.next_in = (Bytef*)(data + 1);
            comp_strm.avail_in = comp_input_size;
            comp_strm.next_out = compressed_buffer;
            comp_strm.avail_out = MAX_BUFFER_SIZE;
            
            ret = deflate(&comp_strm, Z_FINISH);
            
            if (ret == Z_STREAM_END) {
                size_t compressed_size = MAX_BUFFER_SIZE - comp_strm.avail_out;
                
                deflateEnd(&comp_strm);
                
                // Now inflate the properly compressed data
                memset(&strm, 0, sizeof(z_stream));
                ret = inflateInit(&strm);
                if (ret == Z_OK) {
                    strm.next_in = compressed_buffer;
                    strm.avail_in = compressed_size;
                    strm.next_out = output_buffer;
                    strm.avail_out = MAX_BUFFER_SIZE;
                    
                    ret = inflate(&strm, Z_FINISH);
                    
                    inflateEnd(&strm);
                }
            } else {
                deflateEnd(&comp_strm);
            }
        }
    }
    
    return 0;
}