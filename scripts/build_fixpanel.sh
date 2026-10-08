#!/bin/bash
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC="$B/toolchain"
K=$(ls -d "$B"/linux-* | head -1)
O=/mnt/c/distr/soft/r3xclone/lakka-a13/out
T=/mnt/c/Users/INSOER~1/AppData/Local/Temp/opencode/r3x
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
HCF="-march=native -O2 -Wall -pipe -I$TC/include -Wno-format-security -fno-tree-slp-vectorize -fno-tree-vectorize"
LOG=/tmp/fixpanel.log
exec > "$LOG" 2>&1
echo "START $(date)"
cd "$K" || exit 1

echo "== patch lcd0_panel_cfg.c : stop driving button pins =="
python3 - "$K/drivers/video/sunxi/lcd/lcd0_panel_cfg.c" <<'PY'
import sys
F=sys.argv[1]; t=open(F).read()
old='''	for (i = 0; i <= 11; i++)
		a13_pin_out_low(pio, 0x90, i);
	a13_pin_out_low(pio, 0x24, 16); a13_pin_out_low(pio, 0x24, 10);'''
new='''	/* A13FIX: PE0..PE11 + PB16 are the game buttons, do NOT drive them */
	a13_pin_out_low(pio, 0x24, 10);'''
if 'A13FIX' in t:
    print("already patched")
elif old in t:
    t=t.replace(old,new); open(F,'w').write(t); print("patched OK")
else:
    print("PATTERN NOT FOUND"); sys.exit(1)
PY
grep -n "A13FIX\|a13_pin_out_low" "$K/drivers/video/sunxi/lcd/lcd0_panel_cfg.c"

cp -f "$T/a13init" "$B/initramfs/a13init"
cp -f "$T/a13launch" "$B/initramfs/a13launch"
cp -f "$T/a13keys.c" "$K/drivers/input/a13keys.c"
grep -q "a13keys" "$K/drivers/input/Makefile" || echo "obj-y += a13keys.o" >> "$K/drivers/input/Makefile"
rm -f "$K/usr/initramfs_data.cpio.gz" "$K/usr/.initramfs_data.cpio.gz.cmd" "$K/usr/.initramfs_data.cpio.d" "$K/usr/initramfs_data.o" "$K/usr/.initramfs_data.o.cmd"

make ARCH=arm CROSS_COMPILE="$CROSS" HOSTCC="$TC/bin/host-gcc" HOSTCXX="$TC/bin/host-g++" \
  HOSTCFLAGS="$HCF" HOSTLDFLAGS="" HOSTCXXFLAGS="$HCF" -j"$(nproc)" KCFLAGS="-fgnu89-inline" zImage
echo "MAKE RC=$?"
cp -f arch/arm/boot/zImage "$O/lakka-zImage"
"$B/u-boot-2019.04/tools/mkimage" -A arm -O linux -T kernel -C none -a 0x40008000 -e 0x40008000 \
  -n "Lakka A13_4C" -d "$O/lakka-zImage" "$O/lakka-uImage"
echo "MKIMAGE RC=$?"

mkdir -p /mnt/e2
mountpoint -q /mnt/e2 || mount -t drvfs E: /mnt/e2 -o rw
if [ -f /mnt/e2/res/ext/DATA01 ]; then
  cp -f "$O/lakka-uImage" /mnt/e2/res/ext/DATA01
  mkdir -p /mnt/e2/joypads
  cp -f "$T/a13-retro-keys.cfg" /mnt/e2/joypads/a13-retro-keys.cfg
  cp -f "$T/diag.sh" /mnt/e2/diag.sh
  printf 'menu' > /mnt/e2/MODE
  rm -f /mnt/e2/RETRO.LOG /mnt/e2/PSTAT.txt /mnt/e2/DMESG.txt
  sync
  echo "FLASHED"
fi
echo "DONE $(date)"
