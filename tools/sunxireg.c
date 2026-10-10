/*
 * sunxireg - tiny sunxi register access tool (via /dev/mem).
 * Target: Allwinner A13/sun5i. Requires CONFIG_DEVMEM=y and root.
 *
 * usage:
 *   sunxireg rd <hexaddr> [count]      read 32-bit words
 *   sunxireg wr <hexaddr> <hexval>     write 32-bit word
 *   sunxireg pa <0|1>                  set PC10 (speaker PA enable) level
 *   sunxireg pa-state                  print PC CFG1/DAT/PULL for PC10
 *   sunxireg audio [loops] [ms]        dump codec DAC regs (like codecreg)
 *
 * Sunxi PIO base 0x01c20800. Port C: CFG0=0x48, CFG1=0x4c, DAT=0x58,
 * PULL0=0x64, DRV0=0x70. PC10 mux is in CFG1 bits[11:8].
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define PIO_BASE   0x01c20800UL
#define PC_CFG1    (PIO_BASE + 0x4c)
#define PC_DAT     (PIO_BASE + 0x58)
#define PC_PULL0   (PIO_BASE + 0x64)
#define CODEC_BASE 0x01c22c00UL

static int mem_fd;
static volatile unsigned char *map_page(unsigned long phys)
{
	void *p = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED,
		       mem_fd, (off_t)(phys & ~0xfffUL));
	if (p == MAP_FAILED) { perror("mmap"); exit(1); }
	return (volatile unsigned char *)p + (phys & 0xfffUL);
}
static unsigned int rd(unsigned long a){ return *(volatile unsigned int*)map_page(a); }
static void         wr(unsigned long a, unsigned int v){ *(volatile unsigned int*)map_page(a) = v; }

static void pio_pc10_set(int v)
{
	unsigned int cfg, dat, pull;
	cfg  = *(volatile unsigned int*)map_page(PC_CFG1);
	dat  = *(volatile unsigned int*)map_page(PC_DAT);
	pull = *(volatile unsigned int*)map_page(PC_PULL0);
	printf("before: PC_CFG1=%08x PC_DAT=%08x PC_PULL0=%08x\n", cfg, dat, pull);

	cfg = (cfg & ~(0xfU << 8)) | (0x1U << 8);          /* PC10 = output */
	*(volatile unsigned int*)map_page(PC_CFG1) = cfg;
	*(volatile unsigned int*)map_page(PC_PULL0) = (pull & ~(0x3U << 10)) | (0x1U << 10); /* pull-up */

	dat = *(volatile unsigned int*)map_page(PC_DAT);
	if (v) dat |= (1U << 10); else dat &= ~(1U << 10);
	*(volatile unsigned int*)map_page(PC_DAT) = dat;

	cfg  = *(volatile unsigned int*)map_page(PC_CFG1);
	dat  = *(volatile unsigned int*)map_page(PC_DAT);
	pull = *(volatile unsigned int*)map_page(PC_PULL0);
	printf("after : PC_CFG1=%08x PC_DAT=%08x PC_PULL0=%08x  (PC10=%d)\n", cfg, dat, pull, v);
}

static void pio_state(void)
{
	printf("PC_CFG1=%08x PC_DAT=%08x PC_PULL0=%08x PC_DRV0=%08x\n",
	       *(volatile unsigned int*)map_page(PC_CFG1),
	       *(volatile unsigned int*)map_page(PC_DAT),
	       *(volatile unsigned int*)map_page(PC_PULL0),
	       *(volatile unsigned int*)map_page(PIO_BASE + 0x70));
	printf("PC10 mux=%u level=%u\n",
	       (*(volatile unsigned int*)map_page(PC_CFG1) >> 8) & 0xf,
	       (*(volatile unsigned int*)map_page(PC_DAT) >> 10) & 1);
}

static void audio_dump(long loops, int delay_ms)
{
	volatile unsigned char *m = map_page(CODEC_BASE);
	long i;
	printf("# codec @ 0x%08lx loops=%ld delay=%dms\n", (unsigned long)CODEC_BASE, loops, delay_ms);
	printf("# idx   DPC      FIFOC    FIFOS          ACTL     TUNE     DEBUG    TXCNT\n");
	for (i = 0; loops == 0 || i < loops; i++) {
		unsigned int dpc=*(volatile unsigned int*)(m+0x00);
		unsigned int fifoc=*(volatile unsigned int*)(m+0x04);
		unsigned int fifos=*(volatile unsigned int*)(m+0x08);
		unsigned int actl=*(volatile unsigned int*)(m+0x10);
		unsigned int tune=*(volatile unsigned int*)(m+0x14);
		unsigned int dbg=*(volatile unsigned int*)(m+0x18);
		unsigned int txcnt=*(volatile unsigned int*)(m+0x30);
		printf("%3ld  %08x %08x %08x(%3u) %08x %08x %08x %u\n",
		       i, dpc, fifoc, fifos, fifos & 0xff, actl, tune, dbg, txcnt & 0xffffff);
		fflush(stdout);
		if (loops == 0 || i + 1 < loops) usleep(delay_ms * 1000);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) { printf("usage: sunxireg rd <addr> [n] | wr <addr> <val> | pa <0|1> | pa-state | audio [n] [ms]\n"); return 2; }
	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) { perror("open /dev/mem"); return 1; }

	if (!strcmp(argv[1], "rd")) {
		unsigned long a = strtoul(argv[2], 0, 16);
		int n = (argc > 3) ? atoi(argv[3]) : 1, i;
		for (i = 0; i < n; i++) printf("%08lx = %08x\n", a + 4*i, rd(a + 4*i));
	} else if (!strcmp(argv[1], "wr")) {
		unsigned long a = strtoul(argv[2], 0, 16);
		unsigned int v = (unsigned int)strtoul(argv[3], 0, 16);
		wr(a, v);
		printf("%08lx <- %08x (= %08x)\n", a, v, rd(a));
	} else if (!strcmp(argv[1], "pa")) {
		pio_pc10_set(atoi(argv[2]));
	} else if (!strcmp(argv[1], "pa-state")) {
		pio_state();
	} else if (!strcmp(argv[1], "audio")) {
		long l = (argc > 2) ? atol(argv[2]) : 1;
		int d = (argc > 3) ? atoi(argv[3]) : 250;
		audio_dump(l, d);
	} else {
		printf("unknown cmd: %s\n", argv[1]); return 2;
	}
	return 0;
}
