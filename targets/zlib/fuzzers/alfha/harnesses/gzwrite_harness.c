#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <unistd.h>
#include <sys/stat.h>

// Function code from spec
#define FC_ZLIB_GZWRITE 0x1B

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    gzFile file;
    char temp_filename[256];
    int bytes_written;
    unsigned write_size;
    
    // Extract write size from input data
    write_size = (data[0] | (data[1] << 8)) % 1024; // 0 to 1023
    if (write_size == 0) write_size = 1; // Avoid zero writes
    
    size_t write_data_len = size > 4 ? size - 4 : 0;
    const uint8_t* write_data = data + 4;
    
    // Limit write data to avoid excessive operations
    if (write_data_len > write_size) {
        write_data_len = write_size;
    }
    
    // Create a unique temporary filename
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/gzwrite_test_%p.gz", (void*)data);
    
    // Test gzwrite with NULL file handle (should fail gracefully)
    if (write_data_len > 0) {
        bytes_written = gzwrite(NULL, write_data, write_data_len);
        if (bytes_written != 0) {
            // Unexpected success but continue
        }
    }
    
    // Test gzwrite with NULL buffer (should fail gracefully)
    file = gzopen(temp_filename, "wb");
    if (file != Z_NULL) {
        bytes_written = gzwrite(file, NULL, write_size);
        // Should fail with NULL buffer
        gzclose(file);
        unlink(temp_filename);
    }
    
    // Test gzwrite with zero size
    file = gzopen(temp_filename, "wb");
    if (file != Z_NULL) {
        bytes_written = gzwrite(file, write_data, 0);
        // Should return 0 for zero size write
        gzclose(file);
        unlink(temp_filename);
    }
    
    // Test normal write operation
    if (write_data_len > 0) {
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, write_data, write_data_len);
            
            if (bytes_written > 0) {
                // Try writing more data
                bytes_written = gzwrite(file, write_data, write_data_len);
            }
            
            gzclose(file);
            unlink(temp_filename);
        }
    }
    
    // Test writing with various buffer sizes
    if (write_data_len > 0) {
        unsigned test_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
        
        for (int i = 0; i < 10; i++) {
            file = gzopen(temp_filename, "wb");
            if (file != Z_NULL) {
                unsigned size_to_write = test_sizes[i];
                if (size_to_write > write_data_len) size_to_write = write_data_len;
                
                bytes_written = gzwrite(file, write_data, size_to_write);
                gzclose(file);
                unlink(temp_filename);
            }
        }
    }
    
    // Test writing to read-only location (should fail)
    file = gzopen("/proc/version.gz", "wb");
    if (file != Z_NULL) {
        if (write_data_len > 0) {
            bytes_written = gzwrite(file, write_data, write_data_len);
        }
        gzclose(file); // Might fail but shouldn't crash
    }
    
    // Test multiple consecutive writes
    if (write_data_len > 0) {
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            size_t chunk_size = write_data_len > 64 ? 64 : write_data_len;
            
            // Write in small chunks
            for (int i = 0; i < 5; i++) {
                bytes_written = gzwrite(file, write_data, chunk_size);
                if (bytes_written <= 0) break; // Error occurred
            }
            
            gzclose(file);
            unlink(temp_filename);
        }
    }
    
    // Test writing different data patterns
    uint8_t pattern_data[512];
    
    // All zeros pattern
    memset(pattern_data, 0, sizeof(pattern_data));
    file = gzopen(temp_filename, "wb");
    if (file != Z_NULL) {
        bytes_written = gzwrite(file, pattern_data, 256);
        gzclose(file);
        unlink(temp_filename);
    }
    
    // All 0xFF pattern
    memset(pattern_data, 0xFF, sizeof(pattern_data));
    file = gzopen(temp_filename, "wb");
    if (file != Z_NULL) {
        bytes_written = gzwrite(file, pattern_data, 256);
        gzclose(file);
        unlink(temp_filename);
    }
    
    // User data pattern
    if (write_data_len > 0) {
        for (int i = 0; i < 256; i++) {
            pattern_data[i] = write_data[i % write_data_len];
        }
        
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, pattern_data, 256);
            gzclose(file);
            unlink(temp_filename);
        }
    }
    
    // Test writing with different compression levels
    if (write_data_len > 0) {
        char level_filename[256];
        
        for (int level = 1; level <= 9; level++) {
            snprintf(level_filename, sizeof(level_filename), "/tmp/gzwrite_level%d_%p.gz", level, (void*)data);
            
            char mode[8];
            snprintf(mode, sizeof(mode), "wb%d", level);
            
            file = gzopen(level_filename, mode);
            if (file != Z_NULL) {
                bytes_written = gzwrite(file, write_data, write_data_len);
                gzclose(file);
                unlink(level_filename);
            }
        }
    }
    
    // Test appending mode
    if (write_data_len > 0) {
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, write_data, write_data_len / 2);
            gzclose(file);
        }
        
        // Reopen in append mode
        file = gzopen(temp_filename, "ab");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, write_data + (write_data_len / 2), write_data_len - (write_data_len / 2));
            gzclose(file);
        }
        
        unlink(temp_filename);
    }
    
    // Test writing very large data (within reasonable limits)
    if (write_data_len > 0) {
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            // Write the same data multiple times to create larger output
            for (int i = 0; i < 10; i++) {
                bytes_written = gzwrite(file, write_data, write_data_len);
                if (bytes_written <= 0) break;
            }
            gzclose(file);
            unlink(temp_filename);
        }
    }
    
    // Test writing with user-controlled size
    if (write_data_len > 0 && size >= 8) {
        unsigned user_size = (data[2] | (data[3] << 8)) % 1024;
        if (user_size == 0) user_size = 1;
        if (user_size > write_data_len) user_size = write_data_len;
        
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, write_data, user_size);
            gzclose(file);
            unlink(temp_filename);
        }
    }
    
    // Test rapid open/write/close cycles
    if (write_data_len > 0) {
        for (int cycle = 0; cycle < 5; cycle++) {
            char cycle_filename[256];
            snprintf(cycle_filename, sizeof(cycle_filename), "/tmp/gzwrite_cycle%d_%p.gz", cycle, (void*)data);
            
            file = gzopen(cycle_filename, "wb");
            if (file != Z_NULL) {
                size_t cycle_size = write_data_len > 32 ? 32 : write_data_len;
                bytes_written = gzwrite(file, write_data, cycle_size);
                gzclose(file);
                unlink(cycle_filename);
            }
        }
    }
    
    // Test writing binary vs text data
    if (write_data_len >= 16) {
        // Binary mode
        file = gzopen(temp_filename, "wb");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, write_data, 16);
            gzclose(file);
            unlink(temp_filename);
        }
        
        // Text mode (if different)
        file = gzopen(temp_filename, "w");
        if (file != Z_NULL) {
            bytes_written = gzwrite(file, write_data, 16);
            gzclose(file);
            unlink(temp_filename);
        }
    }
    
    // Test error conditions - write after close
    file = gzopen(temp_filename, "wb");
    if (file != Z_NULL) {
        gzclose(file);
        
        // Try to write after close (should fail)
        if (write_data_len > 0) {
            bytes_written = gzwrite(file, write_data, write_data_len);
        }
        
        unlink(temp_filename);
    }
    
    // Test writing to file opened for reading (should fail)
    file = gzopen("/dev/null", "rb");
    if (file != Z_NULL) {
        if (write_data_len > 0) {
            bytes_written = gzwrite(file, write_data, write_data_len);
            // Should fail since file is opened for reading
        }
        gzclose(file);
    }
    
    return 0;
}