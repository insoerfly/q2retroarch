#include "lcd_panel_cfg.h"
#include "../disp/disp_lcd.h"
#include "../disp/ebios_lcdc_tve.h"
#include <linux/io.h>

#define LCD_PARA_USE_CONFIG

#ifdef LCD_PARA_USE_CONFIG
static void LCD_cfg_panel_info(__panel_para_t *info)
{
	memset(info, 0, sizeof(__panel_para_t));
	info->lcd_x = 320;
	info->lcd_y = 240;
	info->lcd_dclk_freq = 5;
	info->lcd_ht = 329;
	info->lcd_hbp = 5;
	info->lcd_hv_hspw = 3;
	info->lcd_vt = 498;
	info->lcd_vbp = 5;
	info->lcd_hv_vspw = 3;
	info->lcd_if = 1;
	info->lcd_cpu_if = 4;
	info->lcd_frm = 2;
	info->lcd_pwm_not_used = 1;
	info->lcd_pwm_ch = 0;
	info->lcd_pwm_freq = 10000;
	info->lcd_pwm_pol = 0;
	info->lcd_io_cfg0 = 0x10000000;
	info->lcd_gamma_correction_en = 0;
}
#endif

static void LCD_panel_init(__u32 sel);
static void LCD_panel_exit(__u32 sel);

static __s32 LCD_open_flow(__u32 sel)
{
	LCD_OPEN_FUNC(sel, LCD_power_on_generic, 50);
	LCD_OPEN_FUNC(sel, TCON_open, 500);
	LCD_OPEN_FUNC(sel, LCD_panel_init, 50);
	LCD_OPEN_FUNC(sel, LCD_bl_open_generic, 0);
	return 0;
}

static __s32 LCD_close_flow(__u32 sel)
{
	LCD_CLOSE_FUNC(sel, LCD_bl_close_generic, 0);
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 0);
	LCD_CLOSE_FUNC(sel, TCON_close, 0);
	LCD_CLOSE_FUNC(sel, LCD_power_off_generic, 1000);
	return 0;
}

#define gc9307_rs(sel, data) LCD_GPIO_write(sel, 0, data)

static void gc9307_write_gram_origin(__u32 sel)
{
	LCD_CPU_WR(sel, 0x0020, 0x0000);
	LCD_CPU_WR(sel, 0x0021, 0x0000);
	LCD_CPU_WR_INDEX(sel, 0x22);
}

static void gc9307_init(__u32 sel)
{
	gc9307_rs(sel, 1); msleep(120);
	gc9307_rs(sel, 0); msleep(100);
	gc9307_rs(sel, 1); msleep(120);
	LCD_CPU_WR_INDEX(sel, 0xfe);
	LCD_CPU_WR_INDEX(sel, 0xef);
	LCD_CPU_WR_INDEX(sel, 0x3a); LCD_CPU_WR_DATA(sel, 0x05);
	LCD_CPU_WR_INDEX(sel, 0x86); LCD_CPU_WR_DATA(sel, 0x98);
	LCD_CPU_WR_INDEX(sel, 0x89); LCD_CPU_WR_DATA(sel, 0x13);
	LCD_CPU_WR_INDEX(sel, 0x8b); LCD_CPU_WR_DATA(sel, 0x80);
	LCD_CPU_WR_INDEX(sel, 0x8d); LCD_CPU_WR_DATA(sel, 0x33);
	LCD_CPU_WR_INDEX(sel, 0x8e); LCD_CPU_WR_DATA(sel, 0x0f);
	LCD_CPU_WR_INDEX(sel, 0xe8); LCD_CPU_WR_DATA(sel, 0x12); LCD_CPU_WR_DATA(sel, 0x00);
	LCD_CPU_WR_INDEX(sel, 0xec); LCD_CPU_WR_DATA(sel, 0x13); LCD_CPU_WR_DATA(sel, 0x02); LCD_CPU_WR_DATA(sel, 0x88);
	LCD_CPU_WR_INDEX(sel, 0xff); LCD_CPU_WR_DATA(sel, 0x62);
	LCD_CPU_WR_INDEX(sel, 0x99); LCD_CPU_WR_DATA(sel, 0x3e);
	LCD_CPU_WR_INDEX(sel, 0x9d); LCD_CPU_WR_DATA(sel, 0x4b);
	LCD_CPU_WR_INDEX(sel, 0x98); LCD_CPU_WR_DATA(sel, 0x3e);
	LCD_CPU_WR_INDEX(sel, 0x9c); LCD_CPU_WR_DATA(sel, 0x4b);
	LCD_CPU_WR_INDEX(sel, 0xc3); LCD_CPU_WR_DATA(sel, 0x27);
	LCD_CPU_WR_INDEX(sel, 0xc4); LCD_CPU_WR_DATA(sel, 0x18);
	LCD_CPU_WR_INDEX(sel, 0xc9); LCD_CPU_WR_DATA(sel, 0x0a);
	LCD_CPU_WR_INDEX(sel, 0xf0); LCD_CPU_WR_DATA(sel, 0x48); LCD_CPU_WR_DATA(sel, 0x0e); LCD_CPU_WR_DATA(sel, 0x0a); LCD_CPU_WR_DATA(sel, 0x0a); LCD_CPU_WR_DATA(sel, 0x06); LCD_CPU_WR_DATA(sel, 0x36);
	LCD_CPU_WR_INDEX(sel, 0xf2); LCD_CPU_WR_DATA(sel, 0x48); LCD_CPU_WR_DATA(sel, 0x0e); LCD_CPU_WR_DATA(sel, 0x0a); LCD_CPU_WR_DATA(sel, 0x0a); LCD_CPU_WR_DATA(sel, 0x06); LCD_CPU_WR_DATA(sel, 0x36);
	LCD_CPU_WR_INDEX(sel, 0xf1); LCD_CPU_WR_DATA(sel, 0x50); LCD_CPU_WR_DATA(sel, 0x8f); LCD_CPU_WR_DATA(sel, 0xaf); LCD_CPU_WR_DATA(sel, 0x3b); LCD_CPU_WR_DATA(sel, 0x3f); LCD_CPU_WR_DATA(sel, 0x7f);
	LCD_CPU_WR_INDEX(sel, 0xf3); LCD_CPU_WR_DATA(sel, 0x50); LCD_CPU_WR_DATA(sel, 0x8f); LCD_CPU_WR_DATA(sel, 0xaf); LCD_CPU_WR_DATA(sel, 0x3b); LCD_CPU_WR_DATA(sel, 0x3f); LCD_CPU_WR_DATA(sel, 0x7f);
	LCD_CPU_WR_INDEX(sel, 0x35); LCD_CPU_WR_DATA(sel, 0x00);
	LCD_CPU_WR_INDEX(sel, 0x44); LCD_CPU_WR_DATA(sel, 0x00); LCD_CPU_WR_DATA(sel, 0x0a);
	LCD_CPU_WR_INDEX(sel, 0x36); LCD_CPU_WR_DATA(sel, 0x28);
	LCD_CPU_WR_INDEX(sel, 0x2a); LCD_CPU_WR_DATA(sel, 0x00); LCD_CPU_WR_DATA(sel, 0x00); LCD_CPU_WR_DATA(sel, 0x01); LCD_CPU_WR_DATA(sel, 0x3f);
	LCD_CPU_WR_INDEX(sel, 0x2b); LCD_CPU_WR_DATA(sel, 0x00); LCD_CPU_WR_DATA(sel, 0x00); LCD_CPU_WR_DATA(sel, 0x00); LCD_CPU_WR_DATA(sel, 0xef);
	LCD_CPU_WR_INDEX(sel, 0x11); msleep(120);
	LCD_CPU_WR_INDEX(sel, 0x29);
	LCD_CPU_WR_INDEX(sel, 0x2c);
}

static void a13_pin_out_low(void __iomem *pio, unsigned base, int pin)
{
	unsigned reg = (unsigned)pin >> 3, nib = (unsigned)pin & 7;
	u32 c = readl(pio + base + reg * 4);
	c = (c & ~(0xFu << (nib * 4))) | (0x1u << (nib * 4));
	writel(c, pio + base + reg * 4);
	writel(readl(pio + base + 0x10) & ~(1u << pin), pio + base + 0x10);
}

static void a13_backlight_on(void)
{
	void __iomem *pio = ioremap(0x01C20800, 0x400);
	int i;
	if (!pio)
		return;
	/* A13FIX: PE0..PE11 + PB16 are the game buttons, do NOT drive them */
	a13_pin_out_low(pio, 0x24, 10);
	a13_pin_out_low(pio, 0x24, 2);  a13_pin_out_low(pio, 0x24, 4);
	a13_pin_out_low(pio, 0x24, 3);  a13_pin_out_low(pio, 0x24, 15);
	a13_pin_out_low(pio, 0xD8, 1);  a13_pin_out_low(pio, 0xD8, 9);
	a13_pin_out_low(pio, 0xD8, 10); a13_pin_out_low(pio, 0xD8, 11);
	a13_pin_out_low(pio, 0xD8, 12);
	a13_pin_out_low(pio, 0x48, 0);  a13_pin_out_low(pio, 0x48, 1);
	a13_pin_out_low(pio, 0x48, 2);  a13_pin_out_low(pio, 0x48, 3);
	a13_pin_out_low(pio, 0x48, 10); a13_pin_out_low(pio, 0x48, 14);
	a13_pin_out_low(pio, 0x48, 15);
	iounmap(pio);
}

static void LCD_panel_init(__u32 sel)
{
	gc9307_init(sel);
	LCD_CPU_AUTO_FLUSH(sel, 1);
	a13_backlight_on();
}

static void LCD_panel_exit(__u32 sel)
{
}

void LCD_get_panel_funs_0(__lcd_panel_fun_t *fun)
{
#ifdef LCD_PARA_USE_CONFIG
	fun->cfg_panel_info = LCD_cfg_panel_info;
#endif
	fun->cfg_open_flow = LCD_open_flow;
	fun->cfg_close_flow = LCD_close_flow;
}
