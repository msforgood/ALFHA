#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATEEND 0x03

#define MAX_BUFFER_SIZE 1024

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    z_stream strm;
    int ret;
    uint8_t input_buffer[MAX_BUFFER_SIZE];
    uint8_t output_buffer[MAX_BUFFER_SIZE];
    
    // Test with NULL pointer (should fail gracefully)
    ret = deflateEnd(NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior, but continue
    }
    
    // Test with uninitialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateEnd(&strm);
    // Should return Z_STREAM_ERROR for uninitialized stream
    
    // Test with properly initialized stream
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        ret = deflateEnd(&strm);
        // Should return Z_OK
    }
    
    // Test with stream that has processed data
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        if (size > 0) {
            size_t input_size = size > MAX_BUFFER_SIZE ? MAX_BUFFER_SIZE : size;
            memcpy(input_buffer, data, input_size);
            
            strm.next_in = input_buffer;
            strm.avail_in = input_size;
            strm.next_out = output_buffer;
            strm.avail_out = MAX_BUFFER_SIZE;
            
            // Process some data
            ret = deflate(&strm, Z_NO_FLUSH);
        }
        
        // End the stream with pending data
        ret = deflateEnd(&strm);
    }
    
    // Test with stream in finished state
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        // Finish compression
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
        
        // Now end the finished stream
        ret = deflateEnd(&strm);
    }
    
    // Test double deflateEnd (should be safe)
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret == Z_OK) {
        ret = deflateEnd(&strm);
        if (ret == Z_OK || ret == Z_DATA_ERROR) {
            // Call deflateEnd again on the same stream
            ret = deflateEnd(&strm);
            // Should handle gracefully
        }
    }
    
    // Test with custom allocators
    if (size >= 4) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Simulate custom allocators scenario
        if (data[0] & 0x01) {
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
        }
        
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        if (ret == Z_OK) {
            ret = deflateEnd(&strm);
        }
    }
    
    // Test with corrupted stream state (but safely)
    if (size >= sizeof(z_stream)) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        if (ret == Z_OK) {
            // Corrupt some non-critical fields
            void *saved_state = strm.state;
            
            // Use input data to modify stream (but preserve critical pointers)
            memcpy((char*)&strm + sizeof(void*), data, 
                   size > (sizeof(z_stream) - sizeof(void*)) ? 
                   (sizeof(z_stream) - sizeof(void*)) : size);
            
            // Restore critical state pointer
            strm.state = saved_state;
            
            ret = deflateEnd(&strm);
        }
    }
    
    // Test various stream states with partial compression
    for (int level = 1; level <= 9; level++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, level);
        if (ret == Z_OK) {
            if (size > 0) {
                size_t chunk_size = (size % 64) + 1;
                if (chunk_size > size) chunk_size = size;
                if (chunk_size > MAX_BUFFER_SIZE) chunk_size = MAX_BUFFER_SIZE;
                
                memcpy(input_buffer, data, chunk_size);
                
                strm.next_in = input_buffer;
                strm.avail_in = chunk_size;
                strm.next_out = output_buffer;
                strm.avail_out = MAX_BUFFER_SIZE;
                
                // Process partial data
                ret = deflate(&strm, Z_PARTIAL_FLUSH);
            }
            
            ret = deflateEnd(&strm);
        }
    }
    
    return 0;
}