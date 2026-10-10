/*
 * codecreg - dump the Allwinner sunxi audio codec DAC registers via /dev/mem.
 *
 * Target: A13 / sun5i, codec base 0x01c22c00.
 * Requires CONFIG_DEVMEM=y and root.
 *
 * Usage: codecreg [loops] [delay_ms]
 *   loops=0 means run forever (until killed).
 *
 * Register map (sound/soc/sunxi/sunxi-codec.h):
 *   0x00 DAC_DPC   DAC enable / digital volume
 *   0x04 DAC_FIFOC FIFO control (DRQ, flush, sample-rate divider, triggers)
 *   0x08 DAC_FIFOS FIFO status (bits[7:0] = sample count in FIFO)
 *   0x0c DAC_TXDATA
 *   0x10 DAC_ACTL  analog control (PA_MUTE, DACAEN_L/R, DACPAS, volume)
 *   0x14 DAC_TUNE
 *   0x18 DAC_DEBUG
 *   0x30 DAC_TXCNT sample counter (bits[23:0])
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define CODEC_PA 0x01c22c00UL

static inline unsigned int rd(volatile unsigned char *b, int off)
{
	return *(volatile unsigned int *)(b + off);
}

int main(int argc, char **argv)
{
	long loops = (argc > 1) ? atol(argv[1]) : 1;
	int delay_ms = (argc > 2) ? atoi(argv[2]) : 250;
	int fd;
	unsigned long page, off;
	volatile unsigned char *base, *m;
	long i;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) { perror("open /dev/mem"); return 1; }

	page = CODEC_PA & ~0xfffUL;
	off  = CODEC_PA - page;
	base = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)page);
	if (base == MAP_FAILED) { perror("mmap"); return 1; }
	m = base + off;

	printf("# codec @ 0x%08lx  loops=%ld delay=%dms\n", (unsigned long)CODEC_PA, loops, delay_ms);
	printf("# idx   DPC      FIFOC    FIFOS          ACTL     TUNE     DEBUG    TXCNT\n");

	for (i = 0; loops == 0 || i < loops; i++) {
		unsigned int dpc = rd(m, 0x00);
		unsigned int fifoc = rd(m, 0x04);
		unsigned int fifos = rd(m, 0x08);
		unsigned int actl = rd(m, 0x10);
		unsigned int tune = rd(m, 0x14);
		unsigned int dbg = rd(m, 0x18);
		unsigned int txcnt = rd(m, 0x30);

		printf("%3ld  %08x %08x %08x(%3u) %08x %08x %08x %u\n",
		       i, dpc, fifoc, fifos, fifos & 0xff, actl, tune, dbg, txcnt & 0xffffff);
		fflush(stdout);

		if (loops == 0 || i + 1 < loops)
			usleep(delay_ms * 1000);
	}
	return 0;
}
