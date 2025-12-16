#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

// Function code from spec
#define FC_ZLIB_DEFLATECOPY 0x04

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2 * sizeof(int)) {
        return 0;
    }
    
    z_stream source_strm, dest_strm;
    int level, strategy;
    int ret;
    
    // Extract parameters from input data
    memcpy(&level, data, sizeof(int));
    memcpy(&strategy, data + sizeof(int), sizeof(int));
    
    // Test with NULL dest pointer (should fail gracefully)
    memset(&source_strm, 0, sizeof(z_stream));
    ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        ret = deflateCopy(NULL, &source_strm);
        if (ret != Z_STREAM_ERROR) {
            // Unexpected behavior for NULL dest
        }
        deflateEnd(&source_strm);
    }
    
    // Test with NULL source pointer (should fail gracefully)
    memset(&dest_strm, 0, sizeof(z_stream));
    ret = deflateCopy(&dest_strm, NULL);
    if (ret != Z_STREAM_ERROR) {
        // Unexpected behavior for NULL source
    }
    
    // Test with uninitialized source stream (should fail)
    memset(&source_strm, 0, sizeof(z_stream));
    memset(&dest_strm, 0, sizeof(z_stream));
    ret = deflateCopy(&dest_strm, &source_strm);
    if (ret != Z_STREAM_ERROR) {
        // Should fail with uninitialized source
    }
    
    // Test self-copy attempt (dest == source)
    memset(&source_strm, 0, sizeof(z_stream));
    ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        ret = deflateCopy(&source_strm, &source_strm);
        if (ret != Z_STREAM_ERROR) {
            // Self-copy should fail
        }
        deflateEnd(&source_strm);
    }
    
    // Test basic successful copy
    memset(&source_strm, 0, sizeof(z_stream));
    memset(&dest_strm, 0, sizeof(z_stream));
    ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        ret = deflateCopy(&dest_strm, &source_strm);
        if (ret == Z_OK) {
            deflateEnd(&dest_strm);
        }
        deflateEnd(&source_strm);
    }
    
    // Test copy with different compression parameters
    int test_levels[] = {0, 1, 6, 9, Z_DEFAULT_COMPRESSION};
    int test_strategies[] = {Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED};
    
    for (int l = 0; l < 5; l++) {
        for (int s = 0; s < 5; s++) {
            memset(&source_strm, 0, sizeof(z_stream));
            memset(&dest_strm, 0, sizeof(z_stream));
            
            ret = deflateInit2(&source_strm, test_levels[l], Z_DEFLATED, 
                              15, 8, test_strategies[s]);
            if (ret == Z_OK) {
                ret = deflateCopy(&dest_strm, &source_strm);
                if (ret == Z_OK) {
                    deflateEnd(&dest_strm);
                }
                deflateEnd(&source_strm);
            }
        }
    }
    
    // Test copy with different window sizes
    int window_sizes[] = {9, 12, 15, -9, -12, -15, 31}; // zlib, raw deflate, gzip
    
    for (int w = 0; w < 7; w++) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          window_sizes[w], 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            deflateEnd(&source_strm);
        }
    }
    
    // Test copy after some compression has occurred
    if (size > 2 * sizeof(int) + 100) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Set up input data
            source_strm.next_in = (Bytef*)(data + 2 * sizeof(int));
            source_strm.avail_in = 100;
            
            // Compress some data
            Bytef output_buffer[512];
            source_strm.next_out = output_buffer;
            source_strm.avail_out = 512;
            
            ret = deflate(&source_strm, Z_NO_FLUSH);
            
            // Now copy the stream
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            deflateEnd(&source_strm);
        }
    }
    
    // Test copy with dictionary set
    if (size > 2 * sizeof(int) + 200) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Set dictionary
            ret = deflateSetDictionary(&source_strm, data + 2 * sizeof(int), 100);
            if (ret == Z_OK) {
                ret = deflateCopy(&dest_strm, &source_strm);
                if (ret == Z_OK) {
                    deflateEnd(&dest_strm);
                }
            }
            deflateEnd(&source_strm);
        }
    }
    
    // Test copy with custom allocators
    if (size > 2 * sizeof(int) + 10) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        // Set custom allocators (using default ones for safety)
        source_strm.zalloc = Z_NULL;
        source_strm.zfree = Z_NULL;
        source_strm.opaque = (voidpf)data; // Use input data as opaque pointer
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            deflateEnd(&source_strm);
        }
    }
    
    // Test multiple sequential copies
    memset(&source_strm, 0, sizeof(z_stream));
    ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        for (int copy_count = 0; copy_count < 3; copy_count++) {
            memset(&dest_strm, 0, sizeof(z_stream));
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
        }
        deflateEnd(&source_strm);
    }
    
    // Test copy chain (copy of copy)
    z_stream second_dest;
    memset(&source_strm, 0, sizeof(z_stream));
    memset(&dest_strm, 0, sizeof(z_stream));
    memset(&second_dest, 0, sizeof(z_stream));
    
    ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
        ret = deflateCopy(&dest_strm, &source_strm);
        if (ret == Z_OK) {
            ret = deflateCopy(&second_dest, &dest_strm);
            if (ret == Z_OK) {
                deflateEnd(&second_dest);
            }
            deflateEnd(&dest_strm);
        }
        deflateEnd(&source_strm);
    }
    
    // Test copy with partially processed input
    if (size > 2 * sizeof(int) + 300) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Set up large input
            source_strm.next_in = (Bytef*)(data + 2 * sizeof(int));
            source_strm.avail_in = 300;
            
            Bytef large_output[1024];
            source_strm.next_out = large_output;
            source_strm.avail_out = 1024;
            
            // Process some input
            ret = deflate(&source_strm, Z_NO_FLUSH);
            
            // Copy in middle of compression
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                // Continue compression on both streams
                Bytef dest_output[512];
                dest_strm.next_out = dest_output;
                dest_strm.avail_out = 512;
                dest_strm.avail_in = 0; // No new input for dest
                
                ret = deflate(&dest_strm, Z_FINISH);
                deflateEnd(&dest_strm);
            }
            
            ret = deflate(&source_strm, Z_FINISH);
            deflateEnd(&source_strm);
        }
    }
    
    // Test copy with corrupted source state (simulate memory corruption)
    if (size > 2 * sizeof(int) + 50) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Save original state
            void *original_state = source_strm.state;
            
            // Temporarily corrupt some fields with input data
            if (size >= 2 * sizeof(int) + sizeof(uLong) * 4) {
                memcpy(&source_strm.total_in, data + 2 * sizeof(int), sizeof(uLong));
                memcpy(&source_strm.total_out, data + 2 * sizeof(int) + sizeof(uLong), sizeof(uLong));
                memcpy(&source_strm.adler, data + 2 * sizeof(int) + 2 * sizeof(uLong), sizeof(uLong));
            }
            
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            
            // Restore state for cleanup
            source_strm.state = original_state;
            deflateEnd(&source_strm);
        }
    }
    
    // Test copy with extreme memory levels
    int mem_levels[] = {1, 9};
    for (int m = 0; m < 2; m++) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                          15, mem_levels[m], Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            deflateEnd(&source_strm);
        }
    }
    
    // Test rapid copy and cleanup to stress memory allocation
    for (int rapid = 0; rapid < 5; rapid++) {
        memset(&source_strm, 0, sizeof(z_stream));
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            memset(&dest_strm, 0, sizeof(z_stream));
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            deflateEnd(&source_strm);
        }
    }
    
    // Test copy after deflateParams call
    if (size > 2 * sizeof(int) + 100) {
        memset(&source_strm, 0, sizeof(z_stream));
        memset(&dest_strm, 0, sizeof(z_stream));
        
        ret = deflateInit2(&source_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            // Set up some input and output
            source_strm.next_in = (Bytef*)(data + 2 * sizeof(int));
            source_strm.avail_in = 50;
            
            Bytef temp_output[256];
            source_strm.next_out = temp_output;
            source_strm.avail_out = 256;
            
            // Compress a bit
            ret = deflate(&source_strm, Z_NO_FLUSH);
            
            // Change parameters
            ret = deflateParams(&source_strm, 9, Z_HUFFMAN_ONLY);
            
            // Now try to copy
            ret = deflateCopy(&dest_strm, &source_strm);
            if (ret == Z_OK) {
                deflateEnd(&dest_strm);
            }
            deflateEnd(&source_strm);
        }
    }
    
    return 0;
}