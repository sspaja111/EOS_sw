/*
 * test_core.c - provera ISA jezgra u korisnickom prostoru (bez kernela).
 * Prevodjenje:  gcc -Wall -o test_core test_core.c
 * Pokretanje :  ./test_core
 */
#include <stdio.h>
#include <stdlib.h>
#include "rv32_core.h"
#include "program.h"

static struct rv32_cpu cpu;

int main(void)
{
	int i, status;

	rv32_reset(&cpu);

	for (i = 0; i < PROGRAM_LEN; i++)
		cpu.imem[i] = program[i];

	status = rv32_run(&cpu, RV_MAX_STEPS);

	printf("status      : %s\n", rv32_status_str(status));
	printf("PC          : 0x%08x\n", cpu.pc);
	printf("instrukcija : %u\n\n", cpu.steps);

	for (i = 0; i < 32; i++)
		printf("x%-2d = 0x%08x (%11d)\n", i, cpu.x[i], (int)cpu.x[i]);

	printf("\nmemorija podataka (prvih 512 B, po 32-bitnoj reci):\n");
	for (i = 0; i < 128; i++) {
		u32 v;
		rv_load(&cpu, i * 4, 4, 0, &v);
		if (v)
			printf("  [0x%04x] = 0x%08x (%d)\n", i * 4, v, (int)v);
	}
	return 0;
}
