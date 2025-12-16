#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATEINIT 0x01

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(int)) {
        return 0;
    }
    
    z_stream strm;
    int level;
    int ret;
    
    // Extract compression level from input data
    memcpy(&level, data, sizeof(int));
    
    // Test with NULL pointer (should fail gracefully)
    ret = deflateInit(NULL, level);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior, but continue testing
    }
    
    // Test with uninitialized stream structure
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, level);
    
    if (ret == Z_OK) {
        // Successful initialization, clean up
        deflateEnd(&strm);
    }
    
    // Test with pre-initialized structure (simulate different initial states)
    if (size >= sizeof(int) + sizeof(void*) * 3) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Simulate custom allocators from input data
        if (data[sizeof(int)] & 0x01) {
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
        }
        
        // Set opaque field from input
        if (data[sizeof(int)] & 0x02) {
            strm.opaque = (voidpf)&strm; // Use safe pointer
        }
        
        ret = deflateInit(&strm, level);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test various compression levels including invalid ones
    int test_levels[] = {
        Z_DEFAULT_COMPRESSION, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        -2, -10, 10, 100, 32767, -32768
    };
    
    for (int i = 0; i < sizeof(test_levels)/sizeof(test_levels[0]); i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = deflateInit(&strm, test_levels[i]);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    // Test with corrupted stream structure (but still allocated)
    if (size >= 16) {
        memset(&strm, 0, sizeof(z_stream));
        // Introduce some corruption from input data
        memcpy(&strm, data + sizeof(int), 
               size - sizeof(int) > sizeof(z_stream) ? sizeof(z_stream) : size - sizeof(int));
        
        // Reset critical pointers to safe values to avoid crashes
        strm.next_in = Z_NULL;
        strm.next_out = Z_NULL;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        ret = deflateInit(&strm, level);
        if (ret == Z_OK) {
            deflateEnd(&strm);
        }
    }
    
    return 0;
}