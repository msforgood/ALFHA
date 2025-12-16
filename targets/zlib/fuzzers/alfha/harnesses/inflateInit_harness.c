#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_INFLATEINIT 0x04

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    z_stream strm;
    int ret;
    
    // Test with NULL pointer (should fail gracefully)
    ret = inflateInit(NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior, but continue testing
    }
    
    // Test with clean uninitialized stream structure
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    
    if (ret == Z_OK) {
        // Successful initialization, clean up
        inflateEnd(&strm);
    }
    
    // Test with various initialization patterns
    if (size >= sizeof(void*) * 4) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Test custom allocators from input data
        if (data[0] & 0x01) {
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
        } else {
            // Use default allocators explicitly
            strm.zalloc = (alloc_func)0;
            strm.zfree = (free_func)0;
        }
        
        // Set opaque field from input
        if (data[0] & 0x02) {
            strm.opaque = (voidpf)&strm; // Use safe pointer
        } else {
            strm.opaque = Z_NULL;
        }
        
        // Set input fields that should be initialized by caller
        if (data[0] & 0x04) {
            strm.next_in = Z_NULL;
            strm.avail_in = 0;
        }
        
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test with pre-corrupted stream (but safe corruption)
    if (size >= 16) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Introduce controlled corruption from input data
        size_t corrupt_size = size > sizeof(z_stream) ? sizeof(z_stream) : size;
        memcpy(&strm, data, corrupt_size);
        
        // Reset critical pointers to safe values to avoid crashes
        strm.next_in = Z_NULL;
        strm.next_out = Z_NULL;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.state = Z_NULL;
        
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test multiple initializations with different patterns
    for (int test_case = 0; test_case < 8; test_case++) {
        memset(&strm, 0, sizeof(z_stream));
        
        switch (test_case) {
            case 0:
                // Clean initialization
                break;
            case 1:
                // Pre-set some fields
                strm.total_in = 12345;
                strm.total_out = 67890;
                break;
            case 2:
                // Set data_type field
                strm.data_type = Z_ASCII;
                break;
            case 3:
                // Set adler field
                strm.adler = 1;
                break;
            case 4:
                // Test with different opaque values
                strm.opaque = (voidpf)data;
                break;
            case 5:
                // Set reserved field (should be ignored)
                strm.reserved = 0x12345678;
                break;
            case 6:
                // Test with non-zero next_in/avail_in
                if (size > 0) {
                    strm.next_in = (Bytef*)data;
                    strm.avail_in = 0; // Safe value
                }
                break;
            case 7:
                // Fill entire structure with pattern
                memset(&strm, 0xAA, sizeof(z_stream));
                // Reset critical pointers
                strm.next_in = Z_NULL;
                strm.next_out = Z_NULL;
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                strm.state = Z_NULL;
                break;
        }
        
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test version compatibility by simulating different scenarios
    if (size >= 4) {
        memset(&strm, 0, sizeof(z_stream));
        
        // Use input data to vary initialization
        uint32_t seed = *(uint32_t*)data;
        
        if (seed & 0x01) {
            strm.zalloc = Z_NULL;
        }
        if (seed & 0x02) {
            strm.zfree = Z_NULL;
        }
        if (seed & 0x04) {
            strm.opaque = (voidpf)(uintptr_t)(seed & 0xFFFF);
        }
        
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    // Test with already initialized stream (should fail or handle gracefully)
    memset(&strm, 0, sizeof(z_stream));
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        // Try to initialize again without ending first
        int ret2 = inflateInit(&strm);
        // Clean up properly
        inflateEnd(&strm);
    }
    
    // Stress test: multiple rapid init/end cycles
    for (int i = 0; i < 10 && i < (int)size; i++) {
        memset(&strm, 0, sizeof(z_stream));
        ret = inflateInit(&strm);
        if (ret == Z_OK) {
            inflateEnd(&strm);
        }
    }
    
    return 0;
}