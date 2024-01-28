#include "encode.c"
#include <stdio.h>

#define BUFSIZE 2880000


int main() {
    char *in = malloc(BUFSIZE);
    char *out = malloc(BUFSIZE);
    char *out2 = malloc(BUFSIZE);
    FILE *_f;
    _f = fopen("data/data_0.bin", "rb");
    if (_f == NULL) {
        printf("! Could not open file.");
        exit(1);
    }
    unsigned long n = fread(in, 1, BUFSIZE, _f);
    if (n == 0) {
        printf("! Could not read file.");
        exit(2);
    }
    int bits = rawToCompressed(in, out, 8000 * 30);
    FILE* _o = fopen("data_0_c.bin", "wb");
    if (_o == NULL) {
        printf("! Could not open file.");
        exit(1);
    }
    printf("bits: %d, bytes: %d\n", bits, bits / 8);
    fwrite(out, 1, bits / 8, _o);
    compressedToRaw(out, out2);
    FILE *_o2 = fopen("data_0.bin", "wb");
    if (_o2 == NULL) {
        printf("! Could not open file.");
        exit(1);
    }
    fwrite(out2, 1, BUFSIZE, _o2);
    return 0;
}
