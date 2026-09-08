/*
 * program_zbb.h - dodatni test program: pokriva Zbb i M instrukcije,
 * granane, skokove i pristup memoriji. Koristi se sa 'app -t'.
 */

#ifndef RV_PROGRAM_ZBB_H
#define RV_PROGRAM_ZBB_H

static const unsigned int program_zbb[] = {
	0xff000093,   /*   0: addi x1, x0, -16 */
	0x00300113,   /*   4: addi x2, x0, 3 */
	0x0f0f11b7,   /*   8: lui  x3, 0x0f0f1 */
	0x2341e193,   /*  12: ori  x3, x3, 0x234 */
	0x60019213,   /*  16: clz   x4, x3 */
	0x60119293,   /*  20: ctz   x5, x3 */
	0x60219313,   /*  24: cpop  x6, x3 */
	0x60409393,   /*  28: sext.b x7, x1 */
	0x60509413,   /*  32: sext.h x8, x1 */
	0x0800c4b3,   /*  36: zext.h x9, x1 */
	0x6981d513,   /*  40: rev8  x10, x3 */
	0x2871d593,   /*  44: orc.b x11, x3 */
	0x60219633,   /*  48: rol  x12, x3, x2 */
	0x6021d6b3,   /*  52: ror  x13, x3, x2 */
	0x0a20c733,   /*  56: min  x14, x1, x2 */
	0x0a20d7b3,   /*  60: minu x15, x1, x2 */
	0x0a20e833,   /*  64: max  x16, x1, x2 */
	0x0a20f8b3,   /*  68: maxu x17, x1, x2 */
	0x4021f933,   /*  72: andn x18, x3, x2 */
	0x4021e9b3,   /*  76: orn  x19, x3, x2 */
	0x4021ca33,   /*  80: xnor x20, x3, x2 */
	0x0220cab3,   /*  84: div  x21, x1, x2 */
	0x0220db33,   /*  88: divu x22, x1, x2 */
	0x0220ebb3,   /*  92: rem  x23, x1, x2 */
	0x0220fc33,   /*  96: remu x24, x1, x2 */
	0x10000c93,   /* 100: addi x25, x0, 256 */
	0x003ca023,   /* 104: sw   x3, 0(x25) */
	0x000cad03,   /* 108: lw   x26, 0(x25) */
	0x000c8d83,   /* 112: lb   x27, 0(x25) */
	0x002cde03,   /* 116: lhu  x28, 2(x25) */
	0x00000e93,   /* 120: addi x29, x0, 0 */
	0x00500f13,   /* 124: addi x30, x0, 5 */
	0x01ee8eb3,   /* 128: loop: add x29, x29, x30 */
	0xffff0f13,   /* 132: addi x30, x30, -1 */
	0xfe0f1ce3,   /* 136: bne x30, x0, loop */
	0x00800fef,   /* 140: jal x31, +8 */
	0x7ff00f93,   /* 144: addi x31, x0, 2047   (preskocena) */
	0x00000073,   /* 148: ecall */
};

#define PROGRAM_ZBB_LEN ((int)(sizeof(program_zbb) / sizeof(program_zbb[0])))

#endif /* RV_PROGRAM_ZBB_H */
