/*
 * program.h - demonstracioni program (isti niz instrukcija koji su kolege
 * koristile za demonstraciju hardverske verzije procesora).
 *
 * Program: puni registre, radi mnozenje (RV32M) i zatim upisuje sadrzaj
 * registara x1..x31 u memoriju podataka, pa se zaustavlja sa ECALL.
 */

#ifndef RV_PROGRAM_H
#define RV_PROGRAM_H

static const unsigned int program[] = {
	0x00000000,   /*   0: nop (0x00000000) */
	0x00500093,   /*   4: addi x1, x0, 5 */
	0x00a00113,   /*   8: addi x2, x0, 10 */
	0x00032437,   /*  12: lui  x8, 0x32 */
	0x000ea4b7,   /*  16: lui  x9, 0xea */
	0x008082b3,   /*  20: add  x5, x1, x8 */
	0x00910333,   /*  24: add  x6, x2, x9 */
	0x022081b3,   /*  28: mul    x3, x1, x2 */
	0x02209233,   /*  32: mulh   x4, x1, x2 */
	0x0262b533,   /*  36: mulhu  x10, x5, x6 */
	0x0262a5b3,   /*  40: mulhsu x11, x5, x6 */
	0x00102023,   /*  44: sw   x1, 0(x0) */
	0x00202823,   /*  48: sw   x2, 16(x0) */
	0x02302023,   /*  52: sw   x3, 32(x0) */
	0x02402823,   /*  56: sw   x4, 48(x0) */
	0x04502023,   /*  60: sw   x5, 64(x0) */
	0x04602823,   /*  64: sw   x6, 80(x0) */
	0x06702023,   /*  68: sw   x7, 96(x0) */
	0x06802823,   /*  72: sw   x8, 112(x0) */
	0x08902023,   /*  76: sw   x9, 128(x0) */
	0x08a02823,   /*  80: sw   x10, 144(x0) */
	0x0ab02023,   /*  84: sw   x11, 160(x0) */
	0x0ac02823,   /*  88: sw   x12, 176(x0) */
	0x0cd02023,   /*  92: sw   x13, 192(x0) */
	0x0ce02823,   /*  96: sw   x14, 208(x0) */
	0x0ef02023,   /* 100: sw   x15, 224(x0) */
	0x0f002823,   /* 104: sw   x16, 240(x0) */
	0x11102023,   /* 108: sw   x17, 256(x0) */
	0x11202823,   /* 112: sw   x18, 272(x0) */
	0x13302023,   /* 116: sw   x19, 288(x0) */
	0x13402823,   /* 120: sw   x20, 304(x0) */
	0x15502023,   /* 124: sw   x21, 320(x0) */
	0x15602823,   /* 128: sw   x22, 336(x0) */
	0x17702023,   /* 132: sw   x23, 352(x0) */
	0x17802823,   /* 136: sw   x24, 368(x0) */
	0x19902023,   /* 140: sw   x25, 384(x0) */
	0x19a02823,   /* 144: sw   x26, 400(x0) */
	0x1bb02023,   /* 148: sw   x27, 416(x0) */
	0x1bc02823,   /* 152: sw   x28, 432(x0) */
	0x1dd02023,   /* 156: sw   x29, 448(x0) */
	0x1de02823,   /* 160: sw   x30, 464(x0) */
	0x1ff02023,   /* 164: sw   x31, 480(x0) */
	0x00000073,   /* 168: ecall */
};

#define PROGRAM_LEN ((int)(sizeof(program) / sizeof(program[0])))

#endif /* RV_PROGRAM_H */
