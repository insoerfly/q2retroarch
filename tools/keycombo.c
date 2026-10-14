/* keycombo - watch the gamepad (/dev/input/js0) and, when SELECT+START+MENU are
 * held for ~2s, create a flag file (used by a13init to enter "Connect to PC").
 * Target: Allwinner A13 (sun5i). Static ARM.
 *
 * usage: keycombo [jsdev] [flagfile] [select] [start] [menu] [hold_ms]
 *   defaults: /dev/input/js0  /flash/.connect_pc  6 7 8  2000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

struct js_event { unsigned int time; short value; unsigned char type; unsigned char number; };

static long now_ms(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (long)t.tv_sec * 1000 + t.tv_usec / 1000;
}

int main(int argc, char **argv)
{
	const char *dev  = (argc > 1) ? argv[1] : "/dev/input/js0";
	const char *flag = (argc > 2) ? argv[2] : "/flash/.connect_pc";
	int sb = (argc > 3) ? atoi(argv[3]) : 6;
	int st = (argc > 4) ? atoi(argv[4]) : 7;
	int mn = (argc > 5) ? atoi(argv[5]) : 8;
	long hold = (argc > 6) ? atol(argv[6]) : 2000;
	unsigned char btn[128];
	long start = 0;
	int fd, i, ready = 0;

	fd = open(dev, O_RDONLY | O_NONBLOCK);
	if (fd < 0) { perror(dev); return 1; }

	memset(btn, 0, sizeof(btn));

	for (;;)
	{
		fd_set rfds;
		struct timeval tv;
		struct js_event e;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;

		if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0)
		{
			while (read(fd, &e, sizeof(e)) == (ssize_t)sizeof(e))
			{
				if (e.type & 0x80)
					continue;                 /* initial-state event */
				if ((e.type & 0x01) && e.number < 128)
					btn[e.number] = e.value ? 1 : 0;
			}
		}

		for (i = 0; i < 128; i++) { /* nothing */ }
		if (btn[sb] && btn[st] && btn[mn])
		{
			if (!start)
				start = now_ms();
			else if (now_ms() - start >= hold)
			{
				FILE *f = fopen(flag, "w");
				if (f) { fputs("1\n", f); fclose(f); }
				close(fd);
				return 0;
			}
		}
		else
			start = 0;
		ready = 1;
	}
	(void)ready;
}
