#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define FC_ZLIB_INFLATESETDICTIONARY 0x28

#define MAX_DICT_SIZE 2048

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(uInt)) {
        return 0;
    }
    
    z_stream strm;
    uInt dictLength;
    int ret;
    
    dictLength = ((uInt*)data)[0];
    size_t data_offset = sizeof(uInt);
    
    if (size <= data_offset) {
        return 0;
    }
    
    // Limit dictionary length to reasonable size
    if (dictLength > size - data_offset) {
        dictLength = size - data_offset;
    }
    if (dictLength > MAX_DICT_SIZE) {
        dictLength = MAX_DICT_SIZE;
    }
    
    // Test with NULL stream pointer (should fail gracefully)
    ret = inflateSetDictionary(NULL, data + data_offset, dictLength);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL stream
    }
    
    // Test with uninitialized stream
    memset(&strm, 0, sizeof(strm));
    ret = inflateSetDictionary(&strm, data + data_offset, dictLength);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for uninitialized stream
    }
    
    // Test with NULL dictionary but non-zero length
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        ret = inflateSetDictionary(&strm, NULL, dictLength);
        inflateEnd(&strm);
    }
    
    // Test with properly initialized stream - standard inflate
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        // Test with zero length dictionary
        ret = inflateSetDictionary(&strm, data + data_offset, 0);
        
        // Test with actual dictionary data
        if (dictLength > 0) {
            ret = inflateSetDictionary(&strm, data + data_offset, dictLength);
        }
        
        inflateEnd(&strm);
    }
    
    // Test with raw inflate (can call anytime)
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit2(&strm, -15); // Raw deflate
    if (ret == Z_OK) {
        if (dictLength > 0) {
            ret = inflateSetDictionary(&strm, data + data_offset, dictLength);
        }
        inflateEnd(&strm);
    }
    
    // Test various dictionary sizes
    if (dictLength > 0) {
        uInt test_sizes[] = {1, dictLength/4, dictLength/2, dictLength, MAX_DICT_SIZE};
        
        for (int i = 0; i < sizeof(test_sizes)/sizeof(test_sizes[0]); i++) {
            if (test_sizes[i] <= dictLength && test_sizes[i] <= size - data_offset) {
                memset(&strm, 0, sizeof(strm));
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                
                ret = inflateInit2(&strm, -15); // Raw deflate
                if (ret == Z_OK) {
                    ret = inflateSetDictionary(&strm, data + data_offset, test_sizes[i]);
                    inflateEnd(&strm);
                }
            }
        }
    }
    
    // Test after Z_NEED_DICT scenario simulation
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
        // Set up input data
        strm.next_in = (Bytef*)(data + data_offset);
        strm.avail_in = size - data_offset;
        
        Bytef output_buffer[256];
        strm.next_out = output_buffer;
        strm.avail_out = sizeof(output_buffer);
        
        // Try inflate - might return Z_NEED_DICT
        ret = inflate(&strm, Z_NO_FLUSH);
        
        if (ret == Z_NEED_DICT && dictLength > 0) {
            // Now set the dictionary
            ret = inflateSetDictionary(&strm, data + data_offset, dictLength);
            if (ret == Z_OK) {
                // Continue inflating
                ret = inflate(&strm, Z_NO_FLUSH);
            }
        }
        
        inflateEnd(&strm);
    }
    
    // Test with corrupted stream state
    memset(&strm, 0xFF, sizeof(strm)); // Fill with invalid data
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.state = NULL; // Force to NULL to avoid crash
    
    ret = inflateSetDictionary(&strm, data + data_offset, dictLength);
    
    // Test multiple dictionary calls
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    ret = inflateInit2(&strm, -15); // Raw deflate
    if (ret == Z_OK) {
        if (dictLength > 8) {
            // Set first dictionary
            ret = inflateSetDictionary(&strm, data + data_offset, dictLength/2);
            
            // Set second dictionary (should replace first)
            ret = inflateSetDictionary(&strm, data + data_offset + dictLength/2, dictLength/2);
        }
        inflateEnd(&strm);
    }
    
    return 0;
}