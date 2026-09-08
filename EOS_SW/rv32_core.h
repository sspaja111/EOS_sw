/*
 * rv32_core.h - softverski model RISC-V procesora (RV32IM + Zbb podskup)
 *
 * Isti izvorni fajl se prevodi i u kernel prostoru (cpu_sw_driver.c)
 * i u korisnickom prostoru (test_core.c), zbog lakseg testiranja.
 *
 * Skup instrukcija je preslikan 1:1 iz RTL opisa procesora:
 *   control_decoder.v, alu_decoder.v, alu.v, immediate.v, branch_module.v
 */

#ifndef RV32_CORE_H
#define RV32_CORE_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/string.h>
#else
#include <stdint.h>
#include <string.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;
typedef int64_t  s64;
#endif

/* Velicine memorija - iste kao u hardverskoj verziji (bram_driver.c) */
#define RV_IMEM_BYTES   1024            /* INSTR_BRAM_SIZE */
#define RV_DMEM_BYTES   16384           /* DATA_BRAM_SIZE  */
#define RV_IMEM_WORDS   (RV_IMEM_BYTES / 4)
#define RV_MAX_STEPS    1000000         /* zastita od beskonacne petlje */

/* Statusi izvrsavanja */
#define RV_OK            0      /* instrukcija izvrsena, nastavlja se     */
#define RV_HALT          1      /* ECALL / EBREAK -> stop_flag            */
#define RV_ERR_ILLEGAL  -1      /* nepoznata instrukcija                  */
#define RV_ERR_MEM      -2      /* pristup van opsega memorije            */
#define RV_ERR_ALIGN    -3      /* neporavnat pristup                     */
#define RV_ERR_PC       -4      /* PC van instrukcijske memorije          */
#define RV_ERR_LIMIT    -5      /* prekoracen maksimalan broj koraka      */

struct rv32_cpu {
	u32 x[32];                      /* registarski fajl x0 - x31   */
	u32 pc;                         /* programski brojac           */
	u32 imem[RV_IMEM_WORDS];        /* instrukcijska memorija      */
	u8  dmem[RV_DMEM_BYTES];        /* memorija podataka           */
	u32 steps;                      /* broj izvrsenih instrukcija  */
	int stop_flag;                  /* 1 posle ECALL/EBREAK        */
	int last_status;                /* poslednji RV_* status       */
	u32 fault_pc;                   /* PC na kome je nastala greska*/
};

/* ------------------------------------------------------------------ */
/* Pomocne funkcije                                                    */
/* ------------------------------------------------------------------ */

static inline u32 rv_sext(u32 val, int bits)
{
	u32 m = 1u << (bits - 1);
	return (val ^ m) - m;
}

static inline u32 rv_clz(u32 v)
{
	u32 n = 0;
	if (!v)
		return 32;
	while (!(v & 0x80000000u)) {
		v <<= 1;
		n++;
	}
	return n;
}

static inline u32 rv_ctz(u32 v)
{
	u32 n = 0;
	if (!v)
		return 32;
	while (!(v & 1u)) {
		v >>= 1;
		n++;
	}
	return n;
}

static inline u32 rv_cpop(u32 v)
{
	u32 n = 0;
	while (v) {
		n += v & 1u;
		v >>= 1;
	}
	return n;
}

static inline u32 rv_rev8(u32 v)
{
	return ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8) |
	       ((v & 0x00ff0000u) >> 8)  | ((v & 0xff000000u) >> 24);
}

static inline u32 rv_orcb(u32 v)
{
	u32 r = 0;
	int i;
	for (i = 0; i < 4; i++)
		if ((v >> (8 * i)) & 0xffu)
			r |= 0xffu << (8 * i);
	return r;
}

static inline u32 rv_rol(u32 v, u32 s)
{
	s &= 31u;
	return s ? ((v << s) | (v >> (32 - s))) : v;
}

static inline u32 rv_ror(u32 v, u32 s)
{
	s &= 31u;
	return s ? ((v >> s) | (v << (32 - s))) : v;
}

/* ------------------------------------------------------------------ */
/* Pristup memoriji podataka (little-endian, kao BRAM sa byte-enable)  */
/* ------------------------------------------------------------------ */

static inline int rv_load(struct rv32_cpu *c, u32 addr, int size, int is_signed,
			  u32 *out)
{
	u32 v = 0;
	int i;

	if (addr > (u32)(RV_DMEM_BYTES - size))
		return RV_ERR_MEM;
	if (addr & (u32)(size - 1))
		return RV_ERR_ALIGN;

	for (i = 0; i < size; i++)
		v |= ((u32)c->dmem[addr + i]) << (8 * i);

	if (is_signed && size < 4)
		v = rv_sext(v, size * 8);

	*out = v;
	return RV_OK;
}

static inline int rv_store(struct rv32_cpu *c, u32 addr, int size, u32 val)
{
	int i;

	if (addr > (u32)(RV_DMEM_BYTES - size))
		return RV_ERR_MEM;
	if (addr & (u32)(size - 1))
		return RV_ERR_ALIGN;

	for (i = 0; i < size; i++)
		c->dmem[addr + i] = (u8)(val >> (8 * i));

	return RV_OK;
}

/* ------------------------------------------------------------------ */
/* Reset                                                               */
/* ------------------------------------------------------------------ */

/* rv32_reset odgovara podizanju reset (stop) flega u hardverskoj verziji:
 * brisu se registri, PC i memorija podataka; instrukcijska memorija se
 * takodje brise da bi svako pokretanje krenulo iz cistog stanja. */
static inline void rv32_reset(struct rv32_cpu *c)
{
	memset(c->x, 0, sizeof(c->x));
	memset(c->dmem, 0, sizeof(c->dmem));
	memset(c->imem, 0, sizeof(c->imem));
	c->pc = 0;
	c->steps = 0;
	c->stop_flag = 1;      /* procesor drzan u resetu dok se ne posalje 'r' */
	c->last_status = RV_OK;
	c->fault_pc = 0;
}

/* ------------------------------------------------------------------ */
/* Izvrsavanje jedne instrukcije                                       */
/* ------------------------------------------------------------------ */

static inline int rv32_step(struct rv32_cpu *c)
{
	u32 instr, opcode, rd, rs1, rs2, funct3, funct7, imm;
	u32 a, b, res = 0, next_pc;
	s32 sa, sb;
	int status;

	if (c->pc > (u32)(RV_IMEM_BYTES - 4) || (c->pc & 3u)) {
		c->fault_pc = c->pc;
		return RV_ERR_PC;
	}

	instr  = c->imem[c->pc >> 2];
	opcode = instr & 0x7fu;
	rd     = (instr >> 7)  & 0x1fu;
	funct3 = (instr >> 12) & 0x07u;
	rs1    = (instr >> 15) & 0x1fu;
	rs2    = (instr >> 20) & 0x1fu;
	funct7 = (instr >> 25) & 0x7fu;

	a  = c->x[rs1];
	b  = c->x[rs2];
	sa = (s32)a;
	sb = (s32)b;
	next_pc = c->pc + 4;

	switch (opcode) {

	/* -------------------------------------------------- LUI */
	case 0x37:
		res = instr & 0xfffff000u;
		break;

	/* ------------------------------------------------ AUIPC */
	case 0x17:
		res = c->pc + (instr & 0xfffff000u);
		break;

	/* -------------------------------------------------- JAL */
	case 0x6f:
		imm = rv_sext(((instr >> 31) & 0x1u) << 20 |
			      ((instr >> 12) & 0xffu) << 12 |
			      ((instr >> 20) & 0x1u) << 11 |
			      ((instr >> 21) & 0x3ffu) << 1, 21);
		res = next_pc;
		next_pc = c->pc + imm;
		break;

	/* ------------------------------------------------- JALR */
	case 0x67:
		imm = rv_sext(instr >> 20, 12);
		res = next_pc;
		next_pc = (a + imm) & ~1u;
		break;

	/* ------------------------------------------- B tip */
	case 0x63: {
		int taken = 0;

		imm = rv_sext(((instr >> 31) & 0x1u) << 12 |
			      ((instr >> 7)  & 0x1u) << 11 |
			      ((instr >> 25) & 0x3fu) << 5 |
			      ((instr >> 8)  & 0xfu) << 1, 13);

		switch (funct3) {
		case 0x0: taken = (a == b); break;              /* beq  */
		case 0x1: taken = (a != b); break;              /* bne  */
		case 0x4: taken = (sa <  sb); break;            /* blt  */
		case 0x5: taken = (sa >= sb); break;            /* bge  */
		case 0x6: taken = (a  <  b); break;             /* bltu */
		case 0x7: taken = (a  >= b); break;             /* bgeu */
		default:
			c->fault_pc = c->pc;
			return RV_ERR_ILLEGAL;
		}
		if (taken)
			next_pc = c->pc + imm;
		c->pc = next_pc;
		c->steps++;
		return RV_OK;                                   /* nema upisa u rd */
	}

	/* ------------------------------------------- LOAD */
	case 0x03: {
		u32 val;
		int size, is_signed;

		imm = rv_sext(instr >> 20, 12);
		switch (funct3) {
		case 0x0: size = 1; is_signed = 1; break;       /* lb  */
		case 0x1: size = 2; is_signed = 1; break;       /* lh  */
		case 0x2: size = 4; is_signed = 0; break;       /* lw  */
		case 0x4: size = 1; is_signed = 0; break;       /* lbu */
		case 0x5: size = 2; is_signed = 0; break;       /* lhu */
		default:
			c->fault_pc = c->pc;
			return RV_ERR_ILLEGAL;
		}
		status = rv_load(c, a + imm, size, is_signed, &val);
		if (status != RV_OK) {
			c->fault_pc = c->pc;
			return status;
		}
		res = val;
		break;
	}

	/* ------------------------------------------ STORE */
	case 0x23: {
		int size;

		imm = rv_sext(((instr >> 25) & 0x7fu) << 5 |
			      ((instr >> 7) & 0x1fu), 12);
		switch (funct3) {
		case 0x0: size = 1; break;                      /* sb */
		case 0x1: size = 2; break;                      /* sh */
		case 0x2: size = 4; break;                      /* sw */
		default:
			c->fault_pc = c->pc;
			return RV_ERR_ILLEGAL;
		}
		status = rv_store(c, a + imm, size, b);
		if (status != RV_OK) {
			c->fault_pc = c->pc;
			return status;
		}
		c->pc = next_pc;
		c->steps++;
		return RV_OK;                                   /* nema upisa u rd */
	}

	/* -------------------------------- OP-IMM (I tip) */
	case 0x13:
		imm = rv_sext(instr >> 20, 12);

		switch (funct3) {
		case 0x0: res = a + imm; break;                         /* addi  */
		case 0x2: res = (sa < (s32)imm) ? 1u : 0u; break;        /* slti  */
		case 0x3: res = (a < imm) ? 1u : 0u; break;              /* sltiu */
		case 0x4: res = a ^ imm; break;                          /* xori  */
		case 0x6: res = a | imm; break;                          /* ori   */
		case 0x7: res = a & imm; break;                          /* andi  */
		case 0x1:
			if (funct7 == 0x30) {                            /* Zbb */
				switch (rs2) {
				case 0x00: res = rv_clz(a); break;       /* clz    */
				case 0x01: res = rv_ctz(a); break;       /* ctz    */
				case 0x02: res = rv_cpop(a); break;      /* cpop   */
				case 0x04: res = rv_sext(a & 0xffu, 8); break;   /* sext.b */
				case 0x05: res = rv_sext(a & 0xffffu, 16); break;/* sext.h */
				default:
					c->fault_pc = c->pc;
					return RV_ERR_ILLEGAL;
				}
			} else if (funct7 == 0x00) {
				res = a << (rs2 & 0x1fu);                /* slli */
			} else {
				c->fault_pc = c->pc;
				return RV_ERR_ILLEGAL;
			}
			break;
		case 0x5:
			if (funct7 == 0x00)
				res = a >> (rs2 & 0x1fu);                /* srli */
			else if (funct7 == 0x20)
				res = (u32)(sa >> (rs2 & 0x1fu));        /* srai */
			else if (funct7 == 0x30)
				res = rv_ror(a, rs2);                    /* rori */
			else if (funct7 == 0x34 && rs2 == 0x18)
				res = rv_rev8(a);                        /* rev8 */
			else if (funct7 == 0x14 && rs2 == 0x07)
				res = rv_orcb(a);                        /* orc.b */
			else {
				c->fault_pc = c->pc;
				return RV_ERR_ILLEGAL;
			}
			break;
		default:
			c->fault_pc = c->pc;
			return RV_ERR_ILLEGAL;
		}
		break;

	/* ------------------------------------ OP (R tip) */
	case 0x33:
		switch (funct7) {
		case 0x00:      /* osnovne RV32I operacije */
			switch (funct3) {
			case 0x0: res = a + b; break;                     /* add  */
			case 0x1: res = a << (b & 0x1fu); break;          /* sll  */
			case 0x2: res = (sa < sb) ? 1u : 0u; break;       /* slt  */
			case 0x3: res = (a < b) ? 1u : 0u; break;         /* sltu */
			case 0x4: res = a ^ b; break;                     /* xor  */
			case 0x5: res = a >> (b & 0x1fu); break;          /* srl  */
			case 0x6: res = a | b; break;                     /* or   */
			case 0x7: res = a & b; break;                     /* and  */
			}
			break;
		case 0x20:      /* sub, sra i Zbb logicke sa invertovanim ulazom */
			switch (funct3) {
			case 0x0: res = a - b; break;                     /* sub  */
			case 0x4: res = ~(a ^ b); break;                  /* xnor */
			case 0x5: res = (u32)(sa >> (b & 0x1fu)); break;  /* sra  */
			case 0x6: res = a | ~b; break;                    /* orn  */
			case 0x7: res = a & ~b; break;                    /* andn */
			default:
				c->fault_pc = c->pc;
				return RV_ERR_ILLEGAL;
			}
			break;
		case 0x01:      /* RV32M */
			switch (funct3) {
			case 0x0:                                          /* mul */
				res = (u32)((s64)sa * (s64)sb);
				break;
			case 0x1:                                          /* mulh */
				res = (u32)(((s64)sa * (s64)sb) >> 32);
				break;
			case 0x2:                                          /* mulhsu */
				res = (u32)(((s64)sa * (s64)(u64)b) >> 32);
				break;
			case 0x3:                                          /* mulhu */
				res = (u32)((((u64)a * (u64)b) >> 32));
				break;
			case 0x4:                                          /* div */
				if (sb == 0)
					res = 0xffffffffu;
				else if (sa == (s32)0x80000000 && sb == -1)
					res = 0x80000000u;
				else
					res = (u32)(sa / sb);
				break;
			case 0x5:                                          /* divu */
				res = (b == 0) ? 0xffffffffu : (a / b);
				break;
			case 0x6:                                          /* rem */
				if (sb == 0)
					res = (u32)sa;
				else if (sa == (s32)0x80000000 && sb == -1)
					res = 0;
				else
					res = (u32)(sa % sb);
				break;
			case 0x7:                                          /* remu */
				res = (b == 0) ? a : (a % b);
				break;
			}
			break;
		case 0x30:      /* rol, ror */
			if (funct3 == 0x1)
				res = rv_rol(a, b);
			else if (funct3 == 0x5)
				res = rv_ror(a, b);
			else {
				c->fault_pc = c->pc;
				return RV_ERR_ILLEGAL;
			}
			break;
		case 0x05:      /* min, minu, max, maxu */
			switch (funct3) {
			case 0x4: res = (sa < sb) ? a : b; break;         /* min  */
			case 0x5: res = (a < b) ? a : b; break;           /* minu */
			case 0x6: res = (sa > sb) ? a : b; break;         /* max  */
			case 0x7: res = (a > b) ? a : b; break;           /* maxu */
			default:
				c->fault_pc = c->pc;
				return RV_ERR_ILLEGAL;
			}
			break;
		case 0x04:      /* zext.h */
			if (funct3 == 0x4 && rs2 == 0x00) {
				res = a & 0xffffu;
			} else {
				c->fault_pc = c->pc;
				return RV_ERR_ILLEGAL;
			}
			break;
		default:
			c->fault_pc = c->pc;
			return RV_ERR_ILLEGAL;
		}
		break;

	/* -------------------------- SYSTEM (ECALL/EBREAK) */
	case 0x73:
		c->stop_flag = 1;
		c->steps++;
		return RV_HALT;

	/* --------------------------------- NOP (sve nule) */
	case 0x00:
		if (instr == 0) {       /* prazna lokacija u instrukcijskoj memoriji */
			c->pc = next_pc;
			c->steps++;
			return RV_OK;
		}
		c->fault_pc = c->pc;
		return RV_ERR_ILLEGAL;

	default:
		c->fault_pc = c->pc;
		return RV_ERR_ILLEGAL;
	}

	if (rd != 0)
		c->x[rd] = res;         /* x0 je uvek 0 */

	c->pc = next_pc;
	c->steps++;
	return RV_OK;
}

/* ------------------------------------------------------------------ */
/* Izvrsavanje celog programa                                          */
/* ------------------------------------------------------------------ */

static inline int rv32_run(struct rv32_cpu *c, u32 max_steps)
{
	int status = RV_OK;
	u32 n = 0;

	c->stop_flag = 0;
	c->steps = 0;

	while (n < max_steps) {
		status = rv32_step(c);
		if (status != RV_OK)
			break;
		n++;
	}

	if (status == RV_OK)
		status = RV_ERR_LIMIT;

	c->last_status = status;
	return status;
}

static inline const char *rv32_status_str(int status)
{
	switch (status) {
	case RV_OK:          return "RUNNING";
	case RV_HALT:        return "HALTED (ECALL/EBREAK)";
	case RV_ERR_ILLEGAL: return "ERROR: illegal instruction";
	case RV_ERR_MEM:     return "ERROR: data memory access out of range";
	case RV_ERR_ALIGN:   return "ERROR: misaligned access";
	case RV_ERR_PC:      return "ERROR: PC out of instruction memory";
	case RV_ERR_LIMIT:   return "ERROR: step limit reached";
	default:             return "ERROR: unknown";
	}
}

#endif /* RV32_CORE_H */
