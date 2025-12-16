#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

// Function code from spec
#define FC_ZLIB_GZREAD 0x1A

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    gzFile file;
    char temp_filename[256];
    uint8_t read_buf[1024];
    int bytes_read;
    unsigned read_size;
    
    // Extract read size from input data
    read_size = (data[0] | (data[1] << 8)) % sizeof(read_buf); // 0 to 1023
    if (read_size == 0) read_size = 1; // Avoid zero reads
    
    size_t file_data_len = size > 4 ? size - 4 : 0;
    const uint8_t* file_data = data + 4;
    
    // Create a unique temporary filename
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/gzread_test_%p.gz", (void*)data);
    
    // Test gzread with NULL file handle (should fail gracefully)
    bytes_read = gzread(NULL, read_buf, read_size);
    if (bytes_read != -1) {
        // Unexpected success but continue
    }
    
    // Test gzread with NULL buffer (should fail gracefully)
    file = gzopen("/dev/null", "rb"); // Safe read-only file
    if (file != Z_NULL) {
        bytes_read = gzread(file, NULL, read_size);
        // Should fail with NULL buffer
        gzclose(file);
    }
    
    // Test gzread with zero size
    file = gzopen("/dev/null", "rb");
    if (file != Z_NULL) {
        bytes_read = gzread(file, read_buf, 0);
        // Should return 0 for zero size read
        gzclose(file);
    }
    
    // Create a temporary file with user data for testing
    if (file_data_len > 0) {
        // First create uncompressed version for gzip compression
        char uncompressed_filename[256];
        snprintf(uncompressed_filename, sizeof(uncompressed_filename), "/tmp/gzread_uncomp_%p", (void*)data);
        
        FILE* temp_file = fopen(uncompressed_filename, "wb");
        if (temp_file) {
            fwrite(file_data, 1, file_data_len, temp_file);
            fclose(temp_file);
            
            // Compress it using gzopen
            gzFile gz_write = gzopen(temp_filename, "wb");
            if (gz_write != Z_NULL) {
                FILE* read_file = fopen(uncompressed_filename, "rb");
                if (read_file) {
                    uint8_t compress_buf[256];
                    size_t bytes;
                    while ((bytes = fread(compress_buf, 1, sizeof(compress_buf), read_file)) > 0) {
                        gzwrite(gz_write, compress_buf, bytes);
                    }
                    fclose(read_file);
                }
                gzclose(gz_write);
            }
            
            unlink(uncompressed_filename); // Clean up uncompressed file
        }
    }
    
    // Test reading from the created compressed file
    if (file_data_len > 0) {
        file = gzopen(temp_filename, "rb");
        if (file != Z_NULL) {
            // Test normal read
            bytes_read = gzread(file, read_buf, read_size);
            
            if (bytes_read > 0) {
                // Try reading again to test continued reading
                bytes_read = gzread(file, read_buf, read_size);
            }
            
            gzclose(file);
        }
    }
    
    // Test reading with various buffer sizes
    if (file_data_len > 0) {
        unsigned test_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
        
        for (int i = 0; i < 11; i++) {
            file = gzopen(temp_filename, "rb");
            if (file != Z_NULL) {
                uint8_t small_buf[1024];
                unsigned size_to_read = test_sizes[i];
                if (size_to_read > sizeof(small_buf)) size_to_read = sizeof(small_buf);
                
                bytes_read = gzread(file, small_buf, size_to_read);
                gzclose(file);
            }
        }
    }
    
    // Test reading from non-existent file
    file = gzopen("/tmp/definitely_does_not_exist_gzread.gz", "rb");
    if (file != Z_NULL) {
        bytes_read = gzread(file, read_buf, read_size);
        gzclose(file);
    }
    
    // Test reading from corrupted/invalid gzip file
    char corrupted_filename[256];
    snprintf(corrupted_filename, sizeof(corrupted_filename), "/tmp/corrupted_%p.gz", (void*)data);
    
    // Create a file with raw input data (likely not valid gzip)
    if (file_data_len > 0) {
        FILE* corrupt_file = fopen(corrupted_filename, "wb");
        if (corrupt_file) {
            fwrite(file_data, 1, file_data_len, corrupt_file);
            fclose(corrupt_file);
            
            // Try to read from corrupted gzip file
            file = gzopen(corrupted_filename, "rb");
            if (file != Z_NULL) {
                // This might fail, but shouldn't crash
                bytes_read = gzread(file, read_buf, read_size);
                
                // Try reading more to test error handling
                if (bytes_read >= 0) {
                    bytes_read = gzread(file, read_buf, read_size);
                }
                
                gzclose(file);
            }
            
            unlink(corrupted_filename);
        }
    }
    
    // Test reading with very large buffer sizes (within limits)
    if (file_data_len > 0) {
        file = gzopen(temp_filename, "rb");
        if (file != Z_NULL) {
            uint8_t large_buf[4096];
            
            // Test large read
            bytes_read = gzread(file, large_buf, sizeof(large_buf));
            
            gzclose(file);
        }
    }
    
    // Test multiple consecutive reads
    if (file_data_len > 0) {
        file = gzopen(temp_filename, "rb");
        if (file != Z_NULL) {
            uint8_t chunk_buf[64];
            
            // Read in small chunks
            for (int i = 0; i < 10; i++) {
                bytes_read = gzread(file, chunk_buf, sizeof(chunk_buf));
                if (bytes_read <= 0) break; // End of file or error
            }
            
            gzclose(file);
        }
    }
    
    // Test reading from empty file
    char empty_filename[256];
    snprintf(empty_filename, sizeof(empty_filename), "/tmp/empty_%p.gz", (void*)data);
    
    gzFile empty_gz = gzopen(empty_filename, "wb");
    if (empty_gz != Z_NULL) {
        gzclose(empty_gz); // Create empty compressed file
        
        file = gzopen(empty_filename, "rb");
        if (file != Z_NULL) {
            bytes_read = gzread(file, read_buf, read_size);
            // Should return 0 for empty file
            gzclose(file);
        }
        
        unlink(empty_filename);
    }
    
    // Test reading after gzeof
    if (file_data_len > 0) {
        file = gzopen(temp_filename, "rb");
        if (file != Z_NULL) {
            uint8_t eof_buf[2048];
            
            // Read everything
            bytes_read = gzread(file, eof_buf, sizeof(eof_buf));
            
            // Try reading past EOF
            bytes_read = gzread(file, read_buf, read_size);
            // Should return 0 or -1
            
            gzclose(file);
        }
    }
    
    // Test reading with user-controlled size
    if (file_data_len > 0 && size >= 8) {
        unsigned user_size = (data[2] | (data[3] << 8)) % 2048;
        if (user_size == 0) user_size = 1;
        
        file = gzopen(temp_filename, "rb");
        if (file != Z_NULL) {
            uint8_t* user_buf = malloc(user_size);
            if (user_buf) {
                bytes_read = gzread(file, user_buf, user_size);
                free(user_buf);
            }
            gzclose(file);
        }
    }
    
    // Test rapid open/read/close cycles
    if (file_data_len > 0) {
        for (int cycle = 0; cycle < 5; cycle++) {
            file = gzopen(temp_filename, "rb");
            if (file != Z_NULL) {
                uint8_t cycle_buf[32];
                bytes_read = gzread(file, cycle_buf, sizeof(cycle_buf));
                gzclose(file);
            }
        }
    }
    
    // Clean up temporary files
    unlink(temp_filename);
    
    return 0;
}