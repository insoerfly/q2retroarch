/*
 * a13keys.c - Allwinner A13 (sun5i) handheld button driver.
 * Maps the PIO controller, polls the buttons and registers an evdev/joydev
 * input device so RetroArch can use them.
 * Pin map from the vendor firmware table g_key_info_a13_x (libsys_a13_a.so).
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#define PIO_BASE   0x01C20800
#define PIO_SIZE   0x400

#define PORT_PB    0x24
#define PORT_PE    0x90

struct a13_btn { unsigned port; unsigned pin; unsigned code; };

static struct a13_btn a13_buttons[] = {
	{ PORT_PE, 11, BTN_DPAD_UP    },  /* up     */
	{ PORT_PE, 10, BTN_DPAD_DOWN  },  /* down   */
	{ PORT_PE,  9, BTN_DPAD_LEFT  },  /* left   */
	{ PORT_PE,  8, BTN_DPAD_RIGHT },  /* right  */
	{ PORT_PE,  2, BTN_SOUTH      },  /* A      */
	{ PORT_PE,  4, BTN_EAST       },  /* B      */
	{ PORT_PE,  3, BTN_NORTH      },  /* X      */
	{ PORT_PE,  5, BTN_WEST       },  /* Y      */
	{ PORT_PE,  7, BTN_TL         },  /* L      */
	{ PORT_PE,  6, BTN_TR         },  /* R      */
	{ PORT_PE,  0, BTN_START      },  /* Start  */
	{ PORT_PB, 16, BTN_SELECT     },  /* Select */
	{ PORT_PE,  1, BTN_MODE       },  /* Menu   */
};
#define NUM_BTNS ARRAY_SIZE(a13_buttons)

static void __iomem *pio;
static struct input_dev *a13_input;
static struct timer_list a13_timer;
static unsigned a13_state;

static inline unsigned port_dat(unsigned port){ return ioread32(pio + port + 0x10); }

static void a13_pin_input_pullup(unsigned port, unsigned pin)
{
	unsigned reg, v;
	reg = port + (pin / 8) * 4;
	v = ioread32(pio + reg);
	v &= ~(0xF << ((pin % 8) * 4));
	iowrite32(v, pio + reg);
	if (pin < 16) {
		v = ioread32(pio + port + 0x1C);
		v &= ~(0x3 << (pin * 2));
		v |=  (0x1 << (pin * 2));
		iowrite32(v, pio + port + 0x1C);
	} else {
		unsigned p = pin - 16;
		v = ioread32(pio + port + 0x20);
		v &= ~(0x3 << (p * 2));
		v |=  (0x1 << (p * 2));
		iowrite32(v, pio + port + 0x20);
	}
}

static void a13_poll(unsigned long __unused)
{
	unsigned i;
	static int _dbg = 0;
	if ((_dbg % 100) == 0)
		pr_info("a13keys: PE=%08x PB=%08x (dbg %d)\n",
			port_dat(PORT_PE), port_dat(PORT_PB), _dbg);
	_dbg++;
	for (i = 0; i < NUM_BTNS; i++) {
		struct a13_btn *b = &a13_buttons[i];
		int pressed = !((port_dat(b->port) >> b->pin) & 1u);   /* active low */
		if (pressed != (int)((a13_state >> i) & 1u)) {
			if (pressed)
				a13_state |= (1u << i);
			else
				a13_state &= ~(1u << i);
			input_report_key(a13_input, b->code, pressed);
			input_sync(a13_input);
			pr_info("a13keys: report code=%u pressed=%d\n", b->code, pressed);
		}
	}
	mod_timer(&a13_timer, jiffies + msecs_to_jiffies(10));
}

static int __init a13keys_init(void)
{
	unsigned i;
	int rc;

	pio = ioremap(PIO_BASE, PIO_SIZE);
	if (!pio)
		return -ENOMEM;

	for (i = 0; i < NUM_BTNS; i++)
		a13_pin_input_pullup(a13_buttons[i].port, a13_buttons[i].pin);

	a13_input = input_allocate_device();
	if (!a13_input) { rc = -ENOMEM; goto err; }
	a13_input->name = "a13-retro-keys";
	a13_input->phys = "a13/input0";
	a13_input->id.bustype = BUS_HOST;
	a13_input->evbit[0] = BIT_MASK(EV_KEY);
	for (i = 0; i < NUM_BTNS; i++)
		set_bit(a13_buttons[i].code, a13_input->keybit);

	rc = input_register_device(a13_input);
	if (rc) goto err_free;

	a13_state = 0;
	setup_timer(&a13_timer, a13_poll, 0);
	mod_timer(&a13_timer, jiffies + msecs_to_jiffies(20));

	pr_info("a13keys: registered %u buttons\n", (unsigned)NUM_BTNS);
	return 0;
err_free:
	input_free_device(a13_input);
err:
	iounmap(pio);
	return rc;
}

static void __exit a13keys_exit(void)
{
	del_timer_sync(&a13_timer);
	input_unregister_device(a13_input);
	iounmap(pio);
}

module_init(a13keys_init);
module_exit(a13keys_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Allwinner A13 handheld buttons");
