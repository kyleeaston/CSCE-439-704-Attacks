#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

unsigned char transform_byte(unsigned char b) {
    return b + 0xA5;
}

char *base64_encode(const unsigned char *data, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz"
                                "0123456789+/";
    if (!data || len == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    /* Base64 output length is 4 * ceil(len/3). +1 for NUL. */
    size_t out_len = 4 * ((len + 2) / 3);
    char *out = malloc(out_len + 1);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = table[(triple >> 18) & 0x3F];
        out[j++] = table[(triple >> 12) & 0x3F];
        out[j++] = table[(triple >> 6)  & 0x3F];
        out[j++] = table[triple & 0x3F];
    }

    /* Add padding '=' if input length not a multiple of 3 */
    size_t mod = len % 3;
    if (mod) {
        /* Replace last 1-2 chars with '=' as needed */
        out[out_len - 1] = '=';
        if (mod == 1) out[out_len - 2] = '=';
    }

    out[out_len] = '\0';
    return out;
}

int main() {

    for (unsigned int i = 1; i < 51; i++){
        char filepath[256];
        
        // make a test dir
        snprintf(filepath, sizeof(filepath), "MALWARE-RAW/%u", i);

        // snprintf(filepath, sizeof(filepath), "MALWARE-RAW%u", i);

        FILE *file = fopen(filepath, "rb");  // Open file in binary mode
        if (!file) {
            fprintf(stderr, "Failed to open file %s: %s\n", filepath, strerror(errno));
            return 1;
        }

        // Find file size
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        // allocate new buffer
        unsigned char *buffer = malloc(size);
        if (!buffer) {
            perror("malloc failed");
            fclose(file);
            return 1;
        }

        // Populate buffer with ceasar cipher
        for (long i = 0; i < size; i++) {
            unsigned char byte;
            if (fread(&byte, 1, 1, file) != 1) {
                fprintf(stderr, "Error reading byte %ld\n", i);
                break;
            }

            buffer[i] = transform_byte(byte);  // change the byte before storing
        }
        fclose(file);

        // XOR-encrypt each byte
        unsigned char key[128] = {
            0x3A, 0x7F, 0xC1, 0x09, 0xB4, 0xE2, 0x58, 0x90,
            0x1D, 0xA7, 0x6C, 0x4B, 0xF3, 0x22, 0x8E, 0x05,
            0x9A, 0xD0, 0x3F, 0x66, 0x11, 0xBE, 0x47, 0x2C,
            0x81, 0xFA, 0x0E, 0x73, 0x4D, 0x95, 0x20, 0x6B,
            0xC6, 0x34, 0xAE, 0x59, 0x02, 0xF8, 0x13, 0xDD,
            0x67, 0xB1, 0x2A, 0x88, 0x5C, 0xE7, 0x30, 0x99,
            0x44, 0x0B, 0xD6, 0xA3, 0x7E, 0x14, 0xCB, 0xF1,
            0x28, 0x86, 0xBD, 0x3C, 0x50, 0x9F, 0xE4, 0x17,
            0x6D, 0xA0, 0x31, 0xC8, 0x4A, 0xF6, 0x02, 0x5B,
            0x93, 0x2F, 0x78, 0xB9, 0x0C, 0xE1, 0x46, 0xAD,
            0x65, 0x3E, 0xB2, 0x0F, 0x87, 0x1A, 0xD9, 0x54,
            0x21, 0xCC, 0x98, 0x7B, 0xFD, 0x40, 0x2D, 0xA6,
            0x5E, 0xB7, 0x14, 0x83, 0x69, 0x0A, 0xF2, 0x3D,
            0xC3, 0x57, 0x8A, 0x10, 0xE8, 0x4F, 0x25, 0x9C,
            0x7A, 0xD4, 0x01, 0x6F, 0xB5, 0x32, 0xEA, 0x48,
            0x0D, 0x91, 0x36, 0xC0, 0xFB, 0x2B, 0x74, 0x62
        };
        for (long i = 0; i < size; i++) {
            buffer[i] ^= key[i%128];      
        }

        // base 64 encode
        char *b64 = base64_encode(buffer, size);
        char output_path[256];
        char* output_dir = "b64";
        snprintf(output_path, sizeof(output_path), "%s/%u.txt", output_dir, i);

        FILE *fout = fopen(output_path, "w");
        if (!fout) {
            perror(output_path);
            continue;
        }
        fprintf(fout, "%s\n", b64);
        fclose(fout);

        free(buffer);
        free(b64);

    }

    return 0;
}
