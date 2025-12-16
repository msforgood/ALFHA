#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <unistd.h>
#include <sys/stat.h>

// Function code from spec
#define FC_ZLIB_GZOPEN 0x17

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    gzFile file;
    char filename[512];
    char mode[32];
    size_t filename_len, mode_len;
    
    // Extract filename and mode from input data
    filename_len = (size_t)(data[0] % 128) + 1; // 1-128 chars
    mode_len = (size_t)(data[1] % 16) + 1; // 1-16 chars
    
    // Ensure we don't exceed buffer or input data
    if (filename_len >= sizeof(filename)) filename_len = sizeof(filename) - 1;
    if (mode_len >= sizeof(mode)) mode_len = sizeof(mode) - 1;
    if (filename_len + mode_len + 2 > size) {
        filename_len = (size - 2) / 2;
        mode_len = (size - 2) - filename_len;
        if (filename_len == 0) filename_len = 1;
        if (mode_len == 0) mode_len = 1;
    }
    
    // Copy filename from input data, ensure null termination
    memcpy(filename, data + 2, filename_len);
    filename[filename_len] = '\0';
    
    // Copy mode from input data, ensure null termination
    memcpy(mode, data + 2 + filename_len, mode_len);
    mode[mode_len] = '\0';
    
    // Test gzopen with NULL filename (should fail gracefully)
    file = gzopen(NULL, "rb");
    if (file != Z_NULL) {
        gzclose(file); // Unexpected success
    }
    
    // Test gzopen with NULL mode (should fail gracefully) 
    file = gzopen("test.gz", NULL);
    if (file != Z_NULL) {
        gzclose(file); // Unexpected success
    }
    
    // Test gzopen with both NULL (should fail gracefully)
    file = gzopen(NULL, NULL);
    if (file != Z_NULL) {
        gzclose(file); // Unexpected success  
    }
    
    // Test with empty filename
    file = gzopen("", "rb");
    if (file != Z_NULL) {
        gzclose(file);
    }
    
    // Test with empty mode
    file = gzopen("test.gz", "");
    if (file != Z_NULL) {
        gzclose(file);
    }
    
    // Test with various invalid mode strings
    const char* invalid_modes[] = {
        "invalid", "x", "zzz", "123", "rbrbwb", "++", "--", 
        "rwrwrw", "abcdef", " rb", "rb ", "\x00", "\n", "\t"
    };
    
    for (int i = 0; i < 15; i++) {
        file = gzopen("/tmp/test_nonexistent.gz", invalid_modes[i]);
        if (file != Z_NULL) {
            gzclose(file);
        }
    }
    
    // Test with various valid mode strings
    const char* valid_modes[] = {
        "r", "rb", "w", "wb", "a", "ab",
        "r1", "r9", "w1", "w9", "rb1", "wb9"
    };
    
    for (int i = 0; i < 12; i++) {
        // Use temp file that doesn't exist for read modes
        if (valid_modes[i][0] == 'r') {
            file = gzopen("/tmp/nonexistent_test_file.gz", valid_modes[i]);
        } else {
            // For write modes, use a temp filename
            char temp_filename[64];
            snprintf(temp_filename, sizeof(temp_filename), "/tmp/gztest_%d.gz", i);
            file = gzopen(temp_filename, valid_modes[i]);
        }
        
        if (file != Z_NULL) {
            gzclose(file);
            // Clean up write mode files
            if (valid_modes[i][0] != 'r') {
                char temp_filename[64];
                snprintf(temp_filename, sizeof(temp_filename), "/tmp/gztest_%d.gz", i);
                unlink(temp_filename);
            }
        }
    }
    
    // Test with user-provided filename (sanitized)
    // Replace dangerous characters to prevent path traversal
    for (size_t i = 0; i < filename_len; i++) {
        if (filename[i] == '/' || filename[i] == '\\' || filename[i] == ':' || 
            filename[i] < 32 || filename[i] > 126) {
            filename[i] = 'X'; // Replace with safe character
        }
    }
    
    // Test user filename with safe prefix
    char safe_filename[256];
    snprintf(safe_filename, sizeof(safe_filename), "/tmp/fuzz_%s", filename);
    
    // Test with user-provided mode (sanitized)
    for (size_t i = 0; i < mode_len; i++) {
        if (mode[i] < 32 || mode[i] > 126) {
            mode[i] = 'r'; // Replace with safe character
        }
    }
    
    file = gzopen(safe_filename, mode);
    if (file != Z_NULL) {
        gzclose(file);
        unlink(safe_filename); // Clean up
    }
    
    // Test with very long filename
    char long_filename[2048];
    memset(long_filename, 'A', sizeof(long_filename) - 1);
    long_filename[sizeof(long_filename) - 1] = '\0';
    strncpy(long_filename, "/tmp/", 5); // Safe prefix
    
    file = gzopen(long_filename, "wb");
    if (file != Z_NULL) {
        gzclose(file);
        unlink(long_filename);
    }
    
    // Test with special characters in filename  
    const char* special_filenames[] = {
        "/tmp/test with spaces.gz",
        "/tmp/test-with-dashes.gz", 
        "/tmp/test_with_underscores.gz",
        "/tmp/test.with.dots.gz",
        "/tmp/TEST_UPPERCASE.GZ",
        "/tmp/test123numbers.gz"
    };
    
    for (int i = 0; i < 6; i++) {
        file = gzopen(special_filenames[i], "wb");
        if (file != Z_NULL) {
            gzclose(file);
            unlink(special_filenames[i]);
        }
    }
    
    // Test opening non-existent files for reading
    file = gzopen("/tmp/definitely_does_not_exist_12345.gz", "rb");
    if (file != Z_NULL) {
        gzclose(file); // Unexpected success
    }
    
    // Test opening read-only location for writing (should fail)
    file = gzopen("/proc/version.gz", "wb");
    if (file != Z_NULL) {
        gzclose(file); // Unexpected success
    }
    
    // Test with compression levels in mode
    char level_mode[8];
    for (int level = 0; level <= 9; level++) {
        snprintf(level_mode, sizeof(level_mode), "wb%d", level);
        char temp_file[64];
        snprintf(temp_file, sizeof(temp_file), "/tmp/level_test_%d.gz", level);
        
        file = gzopen(temp_file, level_mode);
        if (file != Z_NULL) {
            gzclose(file);
            unlink(temp_file);
        }
    }
    
    // Test rapid open/close cycles
    for (int i = 0; i < 10; i++) {
        char cycle_file[64];
        snprintf(cycle_file, sizeof(cycle_file), "/tmp/cycle_%d.gz", i);
        
        file = gzopen(cycle_file, "wb");
        if (file != Z_NULL) {
            gzclose(file);
            unlink(cycle_file);
        }
    }
    
    // Test with binary vs text mode combinations
    const char* binary_modes[] = {"rb", "wb", "ab", "r+b", "w+b"};
    for (int i = 0; i < 5; i++) {
        char bin_file[64];
        snprintf(bin_file, sizeof(bin_file), "/tmp/binary_%d.gz", i);
        
        if (binary_modes[i][0] != 'r') { // Skip read modes
            file = gzopen(bin_file, binary_modes[i]);
            if (file != Z_NULL) {
                gzclose(file);
                unlink(bin_file);
            }
        }
    }
    
    return 0;
}