#ifndef ENCODE_C
#define ENCODE_C
#include <stdio.h>
#include <string.h>

#define COLS 6

typedef struct _FullRep__ {
    unsigned int leader_1 : 1;
    unsigned int data_1 : 4;
    unsigned int leader_2 : 1;
    unsigned int data_2 : 4;
    unsigned int leader_3 : 1;
    unsigned int data_3 : 4;
    unsigned int leader_4 : 1;
    unsigned int data_4 : 5;
    unsigned int _empty : 3;
    unsigned int bits : 8;
} __attribute__((packed)) FullRep;

typedef struct _Encoder__ {
    int last; // last i-th derivative
    char good; // 0 = no fields valid, 1 = last good
} Encoder;


// 17 bit number to FullRep
FullRep svnToFullRep(int n) {
    // Compute the number of bits required to store the integer
    // Since it is signed, it is the largest significance 1 (positive) or 0 (negative) + 1.
    // The integer is only 17 bits so we can start at the 12-16 block (17 is sign so we ignore it).
    char bits = 0;
    char neg = n >> 31;
    for (int i = 16; i > 0; --i) {
        if ((((n & (1 << (i - 1))) >> (i - 1)) ^ neg) & 1) {
            bits = i;
            break;
        }
    }
    // TODO: make not ugly
    printf("bits: %d, neg: %d, exp: %d\n", bits, neg, ((n & (1 << (16 - 1))) >> (16 - 1)) ^ neg);
    FullRep r = { bits > 12, (n >> 13) & 0xf, bits > 8, (n >> 9) & 0xf, bits > 4, (n >> 5) & 0xf, 0, n & 0x1f, 0, (bits > 12) * 5 + (bits > 8) * 5 + (bits > 4) * 5 + 6 };
    printf("%d %d %d %d %d %d %d %d\n", r.leader_1, r.data_1, r.leader_2, r.data_2, r.leader_3, r.data_3, r.leader_4, r.data_4);
    printf("rep bits: %d\n", r.bits);
    return r;
}

int fullRepToSvn(FullRep f) {
    int r = -(f.leader_1 && (f.data_1 >> 3) || (!f.leader_1) && f.leader_2 && (f.data_2 >> 3) || !(f.leader_1 || f.leader_2) && f.leader_3 && (f.data_3 >> 3) || !(f.leader_1 || f.leader_2 || f.leader_3) && (!f.leader_4) && (f.data_4 >> 4));
    printf("%d\n", r);
    r &= -131072; // all ones with 17 zeros at lowest significance
    r |= f.data_4 | (f.data_3 << 5) | (f.data_2 << 9) | (f.data_1 << 13);
    return r;
}

/**
 * Reads little endian.
 * Reads 12 bytes from `buf` and outputs 6 shorts in `out`.
 */
int parseRawLine(short *out, char *buf) {
    for(int b = 0; b < COLS; ++b) {
        out[b] = (short)(((unsigned short)buf[b*2]) + (((unsigned short)buf[b*2+1]) << 8));
        printf("Raw: %d\n", out[b]);
    }
    return 0;
}

int sequenceNext(Encoder *e, short current) {
    if (e->good == 0) {
        e->last = current;
        ++e->good;
        return current;
    }
    int diff = current - e->last;
    e->last = current;
    printf("diff: %d\n", diff);
    return diff;
}

int encodeLine(int *out, short *in, Encoder *ecs) {
    for (int n = 0; n < COLS; ++n) {
        out[n] = sequenceNext(&ecs[n], in[n]);
    }
    return 0;
}

/* Offset must be lest than 8.
 * There must be at least 3 empty bytes in buf.
 * returns bits written.
 */
int writeFullRepWithOffset(char *buf, FullRep f, int offset) {
    char bits = f.bits;
    int mask = 0;
    switch (bits) {
        case 6:
            mask = 0x3f;
            break;
        case 11:
            mask = 0x7ff;
            break;
        case 16:
            mask = 0xffff;
            break;
        case 21:
            mask = 0x1fffff;
            break;
    }
    int data = *((int *) &f);
    data >>= 11;
    data &= mask;
    *((int *)buf) >>= (32 - offset);
    *((int *)buf) <<= (32 - offset);
    *((int *)buf) |= (data & mask) << (32 - offset - bits);
    return bits;
}

/*
 * Offest must be less than 8.
 * Returns bits written.
 */
int writeLine(int *in, char* buf, int offset) {
    int total = 0;
    for (int n = 0; n < COLS; ++n) {
        FullRep f = svnToFullRep(in[n]);
        int bits = writeFullRepWithOffset(buf, f, offset);
        total += bits;
        offset += bits;
        buf += offset / 8;
        offset %= 8;
    }
    return total;
}

/*
 * offset must be less than 8.
 * Returns bits written.
 */
int rawToCompressed(char *in, char *out, unsigned long nlines) {
    int offset = 0;
    unsigned long total = 0;
    Encoder ecs[6];
    int ibuf[6];
    short sbuf[6];
    for (int n = 0; n < nlines; ++n) {
        parseRawLine(sbuf, in+12*n);
        encodeLine(ibuf, sbuf, ecs);
        printf("%d %d\n", *ibuf, *sbuf);
        for (int i = 0; i < COLS; ++i) {
            int bits = writeFullRepWithOffset(out, svnToFullRep(ibuf[i]), offset);
            total += bits;
            offset += bits;
            out += offset / 8;
            offset %= 8;
        }
    }
    return total;
}

#endif
