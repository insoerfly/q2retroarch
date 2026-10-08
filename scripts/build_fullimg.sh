#!/bin/bash
set -e
R=/home/agent/r3x/lakka
B="$R/build.Lakka-A13_4C.arm-3.7.5-devel"
K=$(ls -d "$B"/linux-* | head -1)
O=/mnt/c/distr/soft/r3xclone/lakka-a13/out
T=/mnt/c/Users/INSOER~1/AppData/Local/Temp/opencode/r3x
TC="$B/toolchain"
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
HCF="-march=native -O2 -Wall -pipe -I$TC/include -Wno-format-security -fno-tree-slp-vectorize -fno-tree-vectorize"
MK="$B/u-boot-2019.04/tools/mkimage"
SCRIPTBIN="$R/projects/Allwinner/devices/A13_4C/bootloader/script.bin"
STOCK=/mnt/c/distr/soft/r3xclone/20240628.img
IMG="$O/lakka-full.img"
LOG=/tmp/fullimg.log
exec > "$LOG" 2>&1
echo "START $(date)"

# --- 1) build kernel (our init + driver + fixes already in tree) ---
cd "$K"
cp -f "$T/a13init" "$B/initramfs/a13init"
cp -f "$T/a13keys.c" "$K/drivers/input/a13keys.c"
grep -q a13keys "$K/drivers/input/Makefile" || echo "obj-y += a13keys.o" >> "$K/drivers/input/Makefile"
rm -f "$K/usr/initramfs_data.cpio.gz" "$K/usr/.initramfs_data.cpio.gz.cmd" "$K/usr/.initramfs_data.cpio.d" "$K/usr/initramfs_data.o" "$K/usr/.initramfs_data.o.cmd"
make ARCH=arm CROSS_COMPILE="$CROSS" HOSTCC="$TC/bin/host-gcc" HOSTCXX="$TC/bin/host-g++" \
  HOSTCFLAGS="$HCF" HOSTLDFLAGS="" HOSTCXXFLAGS="$HCF" -j"$(nproc)" KCFLAGS="-fgnu89-inline" zImage
echo "KMAKE RC=$?"
cp -f arch/arm/boot/zImage "$O/lakka-zImage"
"$MK" -A arm -O linux -T kernel -C none -a 0x40008000 -e 0x40008000 -n "A13_4C Lakka" -d "$O/lakka-zImage" "$O/lakka-uImage"
echo "MKIMAGE RC=$?"

# --- 2) boot.scr ---
cat > /tmp/mmcboot.src <<'EOF'
fatload mmc 0:1 0x42000000 res/DATA01
fatload mmc 0:1 0x43000000 res/DATA02
bootm 0x42000000
EOF
"$MK" -A arm -O linux -T script -C none -n "A13_4C mmcboot" -d /tmp/mmcboot.src "$O/boot.scr"

# --- 3) SYSTEM source (prefer card's xz SYSTEM, else built .system) ---
SYSTEM=""
mkdir -p /mnt/e2
for L in F E G H I; do
  mountpoint -q /mnt/e2 && umount /mnt/e2 2>/dev/null
  mount -t drvfs "$L": /mnt/e2 -o rw 2>/dev/null
  if [ -f /mnt/e2/SYSTEM ] && [ -f /mnt/e2/res/ext/DATA01 ]; then SYSTEM="/mnt/e2/SYSTEM"; break; fi
done
[ -z "$SYSTEM" ] && SYSTEM=$(ls "$O"/*.system 2>/dev/null | head -1)
echo "SYSTEM=$SYSTEM"

# --- 4) build image (2600MB, FAT p1 + ext4 p2, stock bootloader) ---
rm -f "$IMG"
dd if=/dev/zero of="$IMG" bs=1M count=2600 status=none
sfdisk "$IMG" <<'PTS'
label: dos
unit: sectors

start=2048, size=3145728, type=c, bootable
start=3147776, size=+, type=83
PTS
dd if="$STOCK" of="$IMG" bs=512 skip=16 seek=16 count=2032 conv=notrunc status=none
sync

L=$(losetup -Pf --show "$IMG")
echo "loop=$L"
partprobe "$L" 2>/dev/null || true
sleep 1
mkfs.vfat -F 32 -n LAKKA "${L}p1"
mkfs.ext4 -q -L LAKKA_DISK "${L}p2"
mkdir -p /mnt/sd1
mount "${L}p1" /mnt/sd1
mkdir -p /mnt/sd1/res /mnt/sd1/joypads
cp "$O/lakka-uImage" /mnt/sd1/res/DATA01
cp "$SCRIPTBIN"     /mnt/sd1/res/DATA02
cp "$O/boot.scr"    /mnt/sd1/boot.scr
cp "$O/retroarch-sunxi" /mnt/sd1/retroarch
cp "$T/a13-retro-keys.cfg" /mnt/sd1/joypads/a13-retro-keys.cfg
echo "copying SYSTEM..."
cp "$SYSTEM" /mnt/sd1/SYSTEM
sync
ls -la /mnt/sd1 /mnt/sd1/res /mnt/sd1/joypads
umount /mnt/sd1
losetup -d "$L"

# --- 5) compress ---
gzip -c "$IMG" > "$IMG.gz"
sha256sum "$IMG" "$IMG.gz"
ls -la "$IMG" "$IMG.gz"
echo "DONE $(date)"
