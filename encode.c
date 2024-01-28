#ifndef ENCODE_C
#define ENCODE_C
#include <stdio.h>
#include <string.h>

//define DEGUB

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

void initECS(Encoder *ecs, unsigned long n) {
    for (unsigned long i = 0; i < n; ++i) {
        ecs[i].good = 0;
    }
}

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
#ifdef DEGUB
    printf("stfr: n: %d, bits: %d\n", n, bits);
#endif
    FullRep r = { bits > 12, (n >> 13) & 0xf, bits > 8, (n >> 9) & 0xf, bits > 4, (n >> 5) & 0xf, 0, n & 0x1f, 0, (bits > 12) * 5 + (bits > 8) * 5 + (bits > 4) * 5 + 6 };
#ifdef DEGUB
    printf("stfr: rep s: %d\n", r.bits);
#endif
    return r;
}

int fullRepToSvn(FullRep f) {
    int r = -(f.leader_1 && (f.data_1 >> 3) || (!f.leader_1) && f.leader_2 && (f.data_2 >> 3) || !(f.leader_1 || f.leader_2) && f.leader_3 && (f.data_3 >> 3) || !(f.leader_1 || f.leader_2 || f.leader_3) && (!f.leader_4) && (f.data_4 >> 4));
    r &= -131072; // all ones with 17 zeros at lowest significance
    r |= f.data_4 | (f.data_3 << 5) | (f.data_2 << 9) | (f.data_1 << 13);
    return r;
}

int fullRepToOpaque(FullRep f) {
#ifdef DEGUB
    printf("frto: f: %d %d, %d %d, %d %d, %d %d, %d\n", f.leader_1, f.data_1, f.leader_2, f.data_2, f.leader_3, f.data_3, f.leader_4, f.data_4, f.bits);
#endif
    int a = *((int *)&f);
#ifdef DEGUB
    printf("frto: a: %x\n", a);
#endif
    char c[4] = { (a & 0xff000000) >> 24, (a & 0xff0000) >> 16, (a & 0xff00) >> 8, a & 0xff };
    int b = *((int *)c);
#ifdef DEGUB
    printf("frto: b: %x\n", b);
#endif
    return b;
}

/**
 * Reads big endian.
 * Reads 12 bytes from `buf` and outputs 6 shorts in `out`.
 */
int parseRawLine(short *out, char *buf) {
    for(int b = 0; b < COLS; ++b) {
        ((char *)out)[b*2] = buf[b*2];
        ((char *)out)[b*2+1] = buf[b*2+1];
    }
    return 0;
}

int sequenceNext(Encoder *e, short current) {
    if (e->good == 0) {
#ifdef DEGUB
        printf("sn: el: %d, eg: %d, c: %d\n", e->last, e->good, current);
#endif
        e->last = current;
        ++e->good;
        return current;
    }
    int diff = current - e->last;
    e->last = current;
#ifdef DEGUB
    printf("sn: el: %d, eg: %d, c: %d, d: %d\n", e->last, e->good, current, diff);
#endif
    return diff;
}

short decompressNext(Encoder *e, int current) {
    if (e->good == 0) {
        e->last = current;
        ++e->good;
        return current;
    }
    short val = current + e->last;
    e->last = val;
    return val;
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
#ifdef DEGUB
    printf("wfro: f: %d %d, %d %d, %d %d, %d %d, %d\n", f.leader_1, f.data_1, f.leader_2, f.data_2, f.leader_3, f.data_3, f.leader_4, f.data_4, f.bits);
    printf("wfro: offset: %d\n", offset);
#endif
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
    int data = fullRepToOpaque(f);
#ifdef DEGUB
    printf("wfro: data: %08x\n", data);
#endif
    data >>= 11;
#ifdef DEGUB
    printf("wfro: data: %08x\n", data);
#endif
    data &= mask;
#ifdef DEGUB
    printf("wfro: data: %08x\n", data);
#endif
    *((int *)buf) >>= (32 - offset);
    *((int *)buf) <<= (32 - offset);
    *((int *)buf) |= (data & mask) << (32 - offset - bits);
#ifdef DEGUB
    printf("wfro: buf: %08x\n", *((int *)buf));
#endif
    return bits;
}

/*
 * offset must be less than 8.
 * Returns bits written.
 */
int rawToCompressed(char *in, char *out, unsigned long nlines) {
    int offset = 0;
    unsigned long total = 0;
    Encoder ecs[6];
    initECS(ecs, 6);
    int ibuf[6];
    short sbuf[6];
    for (int n = 0; n < nlines; ++n) {
        parseRawLine(sbuf, in+12*n);
#ifdef DEGUB
        printf("rtc: in: %d, s: %x\n", *((short *)(in+12*n)), sbuf[0]);
#endif
        encodeLine(ibuf, sbuf, ecs);
        for (int i = 0; i < COLS; ++i) {
            FullRep f = svnToFullRep(ibuf[i]);
            int bits = writeFullRepWithOffset(out, f, offset);
            total += bits;
            offset += bits;
            out += offset / 8;
            offset %= 8;
        }
    }
    return total;
}

/*
 * Returns number of bits read.
 * Offset must be less than 8.
 */
int readSingleFullRep(char *in, int offset, FullRep *f) {
    f->leader_1 = f->leader_2 = f->leader_3 = f->leader_4 = f->data_1 = f->data_2 = f->data_3 = f->data_4 = f->bits = f->_empty = 0;
    char leader = (*((int *) in) & (1 << (32 - offset - 1))) >> (32 - offset - 1);
    if (leader == 0) {
        f->data_4 = (*((int *) in) & (0x1f << (32 - offset - 6))) >> (32 - offset - 6);
        return 6;
    }
    char pack_1 = (*((int *) in) & (0xf << (32 - offset - 5))) >> (32 - offset - 5);
    leader = (*((int *) in) & (1 << (32 - offset - 6))) >> (32 - offset - 6);
    if (leader == 0) {
        f->leader_3 = 1;
        f->data_3 = pack_1;
        f->data_4 = (*((int *) in) & (0x1f << (32 - offset - 11))) >> (32 - offset - 11);
        return 11;
    }
    char pack_2 = (*((int *) in) & (0xf << (32 - offset - 10))) >> (32 - offset - 10);
    leader = (*((int *) in) & (1 << (32 - offset - 11))) >> (32 - offset - 11);
    if (leader == 0) {
        f->leader_2 = f->leader_3 = 1;
        f->data_2 = pack_1;
        f->data_3 = pack_2;
        f->data_4 = (*((int *) in) & (0x1f << (32 - offset - 16))) >> (32 - offset - 16);
        return 16;
    }
    char pack_3 = (*((int *) in) & (0xf << (32 - offset - 15))) >> (32 - offset - 15);
    leader = (*((int *) in) & (1 << (32 - offset - 16))) >> (32 - offset - 16);
    if (leader == 0) {
        f->leader_1 = f->leader_2 = f->leader_3 = 1;
        f->data_1 = pack_1;
        f->data_2 = pack_2;
        f->data_3 = pack_3;
        f->data_4 = (*((int *) in) & (0x1f << (32 - offset - 21))) >> (32 - offset - 21);
        return 21;
    } else {
        printf("! Invalid final leader.");
    }
    return 0;
}

int compressedToRaw(char *in, char *out) {
    int offset = 0;
    Encoder ecs[6];
    initECS(ecs, 6);
    FullRep f;
    short s;
    for (int i = 0; i < 30 * 8000 * 6; ++i) {
        offset += readSingleFullRep(in, offset, &f);
        // printf("Read Single: %d %d %d %d %d %d %d %d\n", f.leader_1, f.data_1, f.leader_2, f.data_2, f.leader_3, f.data_3, f.leader_4, f.data_4);
        in += offset / 8;
        offset %= 8;
        s = decompressNext(&ecs[i%6], fullRepToSvn(f));
        // printf("svn: %d, s: %d\n", fullRepToSvn(f), s);
        ((short *)out)[i] = s;
    }
    return 0;
}

#endif
