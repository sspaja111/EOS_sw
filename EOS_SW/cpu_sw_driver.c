/*
 * cpu_sw_driver.c - "softverska" verzija RISC-V procesora
 *
 * Kompletno izvrsavanje instrukcija se odvija u kernel prostoru. Hardver
 * (FPGA, AXI, BRAM) ne ucestvuje ni u jednom koraku - instrukcijska memorija,
 * memorija podataka, registarski fajl i ALU su realizovani kao strukture
 * podataka u kernel modulu.
 *
 * Kreiraju se dva char uredjaja:
 *   /dev/cpu_sw      (minor 0) - upravljanje procesorom + ispis registara
 *   /dev/cpu_sw_mem  (minor 1) - ispis memorije podataka
 *
 * Komande koje se upisuju u /dev/cpu_sw (ASCII, kao u hardverskoj verziji):
 *   "s"                  - reset (stop flag = 1, brise registre i memorije)
 *   "i <adresa> <rijec>"   - upis instrukcije u instrukcijsku memoriju
 *   "0 <adresa> <rijec>"   - isto (format kompatibilan sa bram_driver-om)
 *   "d <adresa> <rijec>"   - upis 32-bitne reci u memoriju podataka
 *   "r"                  - pokretanje programa do ECALL/EBREAK
 *   "n [broj]"           - izvrsavanje jednog ili N koraka (single step)
 *
 * Citanje iz /dev/cpu_sw vraca stanje svih 32 registra, PC, broj izvrsenih
 * instrukcija i status zaustavljanja.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/version.h>

#include "rv32_core.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Softverska (kernel) implementacija RISC-V procesora");

#define DRIVER_NAME       "cpu_sw"
#define MAX_DEVICES       2
#define WRITE_BUFF_SIZE   64
#define DUMP_BUFF_SIZE    4096
#define DMEM_DUMP_BYTES   512     /* koliko bajtova memorije podataka se ispisuje */

/* ------------------------------------------------------------------ */
/* Globalno stanje modula                                              */
/* ------------------------------------------------------------------ */

static dev_t my_dev_id;
static struct class *my_class;
static struct cdev *my_cdev;

static struct rv32_cpu *cpu;            /* stanje softverskog procesora */
static DEFINE_MUTEX(cpu_lock);          /* zastita od paralelnog pristupa */

/* ------------------------------------------------------------------ */
/* Izvrsavanje programa                                                */
/* ------------------------------------------------------------------ */

/* Izvrsava najvise max_steps instrukcija. cond_resched() se poziva
 * periodicno da duzi program ne bi blokirao kernel. */
static int cpu_sw_execute(u32 max_steps)
{
	int status = RV_OK;
	u32 n = 0;

	cpu->stop_flag = 0;

	while (n < max_steps) {
		status = rv32_step(cpu);
		n++;
		if (status != RV_OK)
			break;
		if ((n & 0xfffu) == 0)
			cond_resched();
	}

	if (status == RV_OK && n >= max_steps)
		status = RV_ERR_LIMIT;

	cpu->last_status = status;
	return status;
}

/* ------------------------------------------------------------------ */
/* Formatiranje ispisa                                                 */
/* ------------------------------------------------------------------ */

/* Imena registara po RISC-V ABI konvenciji - radi citljivijeg ispisa */
static const char * const reg_names[32] = {
	"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
	"s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
	"a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
	"s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

static int cpu_sw_dump_regs(char *buf, size_t size)
{
	int len = 0;
	int i;

	len += scnprintf(buf + len, size - len,
			 "=== SOFTVERSKI RISC-V PROCESOR - STANJE ===\n");
	len += scnprintf(buf + len, size - len,
			 "status      : %s\n", rv32_status_str(cpu->last_status));
	len += scnprintf(buf + len, size - len,
			 "stop_flag   : %d\n", cpu->stop_flag);
	len += scnprintf(buf + len, size - len,
			 "PC          : 0x%08x\n", cpu->pc);
	len += scnprintf(buf + len, size - len,
			 "instrukcija : %u\n", cpu->steps);
	if (cpu->last_status < 0)
		len += scnprintf(buf + len, size - len,
				 "greska na PC: 0x%08x\n", cpu->fault_pc);
	len += scnprintf(buf + len, size - len,
			 "-------------------------------------------\n");

	for (i = 0; i < 32; i++)
		len += scnprintf(buf + len, size - len,
				 "x%-2d (%-4s) = 0x%08x  %11d\n",
				 i, reg_names[i], cpu->x[i], (int)cpu->x[i]);

	len += scnprintf(buf + len, size - len,
			 "===========================================\n");
	return len;
}

static int cpu_sw_dump_mem(char *buf, size_t size)
{
	int len = 0;
	u32 addr;

	for (addr = 0; addr < DMEM_DUMP_BYTES; addr += 4) {
		u32 v = (u32)cpu->dmem[addr] |
			((u32)cpu->dmem[addr + 1] << 8) |
			((u32)cpu->dmem[addr + 2] << 16) |
			((u32)cpu->dmem[addr + 3] << 24);

		len += scnprintf(buf + len, size - len, "%08x\n", v);
		if (len >= (int)size - 16)
			break;
	}
	return len;
}

/* ------------------------------------------------------------------ */
/* File operations                                                     */
/* ------------------------------------------------------------------ */

struct dump_state {
	char *buf;
	size_t len;
};

static int cpu_sw_open(struct inode *pinode, struct file *pfile)
{
	struct dump_state *st;
	int minor = iminor(pinode);

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->buf = kzalloc(DUMP_BUFF_SIZE, GFP_KERNEL);
	if (!st->buf) {
		kfree(st);
		return -ENOMEM;
	}

	/* Snimak stanja se pravi u trenutku otvaranja fajla, pa se citanje
	 * moze obaviti u proizvoljnom broju poziva read(). */
	mutex_lock(&cpu_lock);
	if (minor == 1)
		st->len = cpu_sw_dump_mem(st->buf, DUMP_BUFF_SIZE);
	else
		st->len = cpu_sw_dump_regs(st->buf, DUMP_BUFF_SIZE);
	mutex_unlock(&cpu_lock);

	pfile->private_data = st;
	return 0;
}

static int cpu_sw_close(struct inode *pinode, struct file *pfile)
{
	struct dump_state *st = pfile->private_data;

	if (st) {
		kfree(st->buf);
		kfree(st);
		pfile->private_data = NULL;
	}
	return 0;
}

static ssize_t cpu_sw_read(struct file *pfile, char __user *buffer,
			   size_t length, loff_t *offset)
{
	struct dump_state *st = pfile->private_data;

	if (!st)
		return -EFAULT;

	return simple_read_from_buffer(buffer, length, offset, st->buf, st->len);
}

static ssize_t cpu_sw_write(struct file *pfile, const char __user *buffer,
			    size_t length, loff_t *offset)
{
	char buff[WRITE_BUFF_SIZE];
	unsigned int addr, value, steps;
	int status, dev;
	ssize_t ret = length;

	if (length == 0 || length >= WRITE_BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buff, buffer, length))
		return -EFAULT;
	buff[length] = '\0';

	mutex_lock(&cpu_lock);

	switch (buff[0]) {

	case 's':       /* reset */
		rv32_reset(cpu);
		printk(KERN_INFO "cpu_sw: reset - procesor je zaustavljen\n");
		break;

	case 'r':       /* run */
		cpu->pc = 0;
		cpu->steps = 0;
		status = cpu_sw_execute(RV_MAX_STEPS);
		printk(KERN_INFO "cpu_sw: izvrseno %u instrukcija, %s\n",
		       cpu->steps, rv32_status_str(status));
		break;

	case 'n':       /* single / N step */
		steps = 1;
		if (sscanf(buff, "n %u", &steps) != 1)
			steps = 1;
		status = cpu_sw_execute(steps);
		printk(KERN_INFO "cpu_sw: korak, PC = 0x%08x, %s\n",
		       cpu->pc, rv32_status_str(status));
		break;

	case 'i':       /* upis instrukcije: "i <adresa> <rec>" */
		if (sscanf(buff, "i %u %u", &addr, &value) != 2) {
			ret = -EINVAL;
			break;
		}
		if (addr > RV_IMEM_BYTES - 4 || (addr & 3u)) {
			printk(KERN_ERR "cpu_sw: neispravna adresa instrukcije %u\n",
			       addr);
			ret = -EINVAL;
			break;
		}
		cpu->imem[addr >> 2] = value;
		break;

	case 'd':       /* upis u memoriju podataka: "d <adresa> <rec>" */
		if (sscanf(buff, "d %u %u", &addr, &value) != 2) {
			ret = -EINVAL;
			break;
		}
		if (rv_store(cpu, addr, 4, value) != RV_OK) {
			printk(KERN_ERR "cpu_sw: neispravna adresa podatka %u\n",
			       addr);
			ret = -EINVAL;
		}
		break;

	default:
		/* Format kompatibilan sa bram_driver-om: "<dev> <adresa> <rec>",
		 * gde je dev 0 = instrukcijska, 1 = memorija podataka. */
		if (sscanf(buff, "%d %u %u", &dev, &addr, &value) == 3) {
			if (dev == 0 && addr <= RV_IMEM_BYTES - 4 && !(addr & 3u)) {
				cpu->imem[addr >> 2] = value;
			} else if (dev == 1 &&
				   rv_store(cpu, addr, 4, value) == RV_OK) {
				/* ok */
			} else {
				ret = -EINVAL;
			}
		} else {
			printk(KERN_ERR "cpu_sw: nepoznata komanda \"%s\"\n", buff);
			ret = -EINVAL;
		}
		break;
	}

	mutex_unlock(&cpu_lock);
	return ret;
}

static const struct file_operations my_fops = {
	.owner   = THIS_MODULE,
	.open    = cpu_sw_open,
	.read    = cpu_sw_read,
	.write   = cpu_sw_write,
	.release = cpu_sw_close,
};

/* ------------------------------------------------------------------ */
/* Init / exit                                                         */
/* ------------------------------------------------------------------ */

static int __init cpu_sw_init(void)
{
	int ret;

	cpu = vzalloc(sizeof(*cpu));
	if (!cpu)
		return -ENOMEM;
	rv32_reset(cpu);

	ret = alloc_chrdev_region(&my_dev_id, 0, MAX_DEVICES, DRIVER_NAME);
	if (ret) {
		printk(KERN_ERR "cpu_sw: neuspesna registracija char uredjaja\n");
		goto fail_alloc;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	my_class = class_create("cpu_sw_class");
#else
	my_class = class_create(THIS_MODULE, "cpu_sw_class");
#endif
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		goto fail_class;
	}

	if (IS_ERR(device_create(my_class, NULL, MKDEV(MAJOR(my_dev_id), 0),
				 NULL, "cpu_sw"))) {
		ret = -ENODEV;
		goto fail_dev0;
	}
	if (IS_ERR(device_create(my_class, NULL, MKDEV(MAJOR(my_dev_id), 1),
				 NULL, "cpu_sw_mem"))) {
		ret = -ENODEV;
		goto fail_dev1;
	}

	my_cdev = cdev_alloc();
	if (!my_cdev) {
		ret = -ENOMEM;
		goto fail_cdev;
	}
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;

	ret = cdev_add(my_cdev, my_dev_id, MAX_DEVICES);
	if (ret) {
		cdev_del(my_cdev);
		goto fail_cdev;
	}

	printk(KERN_INFO "cpu_sw: softverski RISC-V procesor spreman "
			 "(imem %d B, dmem %d B)\n",
	       RV_IMEM_BYTES, RV_DMEM_BYTES);
	return 0;

fail_cdev:
	device_destroy(my_class, MKDEV(MAJOR(my_dev_id), 1));
fail_dev1:
	device_destroy(my_class, MKDEV(MAJOR(my_dev_id), 0));
fail_dev0:
	class_destroy(my_class);
fail_class:
	unregister_chrdev_region(my_dev_id, MAX_DEVICES);
fail_alloc:
	vfree(cpu);
	return ret;
}

static void __exit cpu_sw_exit(void)
{
	int i;

	cdev_del(my_cdev);
	for (i = 0; i < MAX_DEVICES; i++)
		device_destroy(my_class, MKDEV(MAJOR(my_dev_id), i));
	class_destroy(my_class);
	unregister_chrdev_region(my_dev_id, MAX_DEVICES);
	vfree(cpu);
	printk(KERN_INFO "cpu_sw: modul uklonjen\n");
}

module_init(cpu_sw_init);
module_exit(cpu_sw_exit);
