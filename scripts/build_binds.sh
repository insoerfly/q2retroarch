#!/bin/bash
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC="$B/toolchain"
K=$(ls -d "$B"/linux-* | head -1)
O=/mnt/c/distr/soft/r3xclone/lakka-a13/out
T=/mnt/c/Users/INSOER~1/AppData/Local/Temp/opencode/r3x
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
HCF="-march=native -O2 -Wall -pipe -I$TC/include -Wno-format-security -fno-tree-slp-vectorize -fno-tree-vectorize"
LOG=/tmp/binds.log
exec > "$LOG" 2>&1
echo "START $(date)"
cd "$K" || exit 1
cp -f "$T/a13init" "$B/initramfs/a13init"
cp -f "$T/a13keys.c" "$K/drivers/input/a13keys.c"
grep -q "a13keys" "$K/drivers/input/Makefile" || echo "obj-y += a13keys.o" >> "$K/drivers/input/Makefile"
rm -f "$K/usr/initramfs_data.cpio.gz" "$K/usr/.initramfs_data.cpio.gz.cmd" "$K/usr/.initramfs_data.cpio.d" "$K/usr/initramfs_data.o" "$K/usr/.initramfs_data.o.cmd"
make ARCH=arm CROSS_COMPILE="$CROSS" HOSTCC="$TC/bin/host-gcc" HOSTCXX="$TC/bin/host-g++" \
  HOSTCFLAGS="$HCF" HOSTLDFLAGS="" HOSTCXXFLAGS="$HCF" -j"$(nproc)" KCFLAGS="-fgnu89-inline" zImage
echo "KMAKE RC=$?"
cp -f arch/arm/boot/zImage "$O/lakka-zImage"
"$B/u-boot-2019.04/tools/mkimage" -A arm -O linux -T kernel -C none -a 0x40008000 -e 0x40008000 \
  -n "Lakka A13_4C" -d "$O/lakka-zImage" "$O/lakka-uImage"

mkdir -p /mnt/e2
CARD=""
for L in F E G H I; do
  mountpoint -q /mnt/e2 && umount /mnt/e2 2>/dev/null
  mount -t drvfs "$L": /mnt/e2 -o rw 2>/dev/null
  if [ -f /mnt/e2/res/ext/DATA01 ]; then CARD="$L:"; break; fi
done
if [ -n "$CARD" ]; then
  echo "card=$CARD"
  cp -f "$O/lakka-uImage" /mnt/e2/res/ext/DATA01
  mkdir -p /mnt/e2/joypads; cp -f "$T/a13-retro-keys.cfg" /mnt/e2/joypads/
  rm -f /mnt/e2/RETRO.LOG /mnt/e2/RA.LOG /mnt/e2/DMESG.txt /mnt/e2/.expanding
  sync; echo "FLASHED"
else echo "no card"; fi
echo "DONE $(date)"
