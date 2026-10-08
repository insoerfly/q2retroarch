#!/bin/bash
# build_sdimage.sh — собрать полный SD-образ Lakka/RetroArch для A13 (Q2/A13_4C).
# Требует: референсный загрузчик bootloader/boot0-boot1_8k-1mb.bin, uImage, SYSTEM, retroarch, script.bin.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)

OUT=${OUT:-$REPO/out}
IMG=${IMG:-$OUT/lakka-full.img}
KERNEL_IMG=${KERNEL_IMG:-$OUT/lakka-uImage}
SYSTEM=${SYSTEM:-$OUT/SYSTEM}
RETROARCH=${RETROARCH:-$OUT/retroarch}
SCRIPTBIN=${SCRIPTBIN:-$REPO/bootloader/script.bin}
BOOTLOADER=${BOOTLOADER:-$REPO/bootloader/boot0-boot1_8k-1mb.bin}
MKIMAGE=${MKIMAGE:-mkimage}

for f in "$KERNEL_IMG" "$SYSTEM" "$SCRIPTBIN" "$BOOTLOADER"; do
  [ -f "$f" ] || { echo "НЕТ ФАЙЛА: $f"; exit 1; }
done

mkdir -p "$OUT"

echo "== boot.scr =="
cat > /tmp/mmcboot.src <<'EOF'
fatload mmc 0:1 0x42000000 res/DATA01
fatload mmc 0:1 0x43000000 res/DATA02
bootm 0x42000000
EOF
"$MKIMAGE" -A arm -O linux -T script -C none -n "A13_4C mmcboot" -d /tmp/mmcboot.src "$OUT/boot.scr"

echo "== создаю образ 2600MB =="
rm -f "$IMG"
dd if=/dev/zero of="$IMG" bs=1M count=2600 status=none
sfdisk "$IMG" <<'PTS'
label: dos
unit: sectors

start=2048, size=3145728, type=c, bootable
start=3147776, size=+, type=83
PTS

echo "== загрузчик (секторы 16..2047) =="
dd if="$BOOTLOADER" of="$IMG" bs=512 seek=16 conv=notrunc status=none
sync

echo "== ФС + файлы =="
L=$(losetup -Pf --show "$IMG")
partprobe "$L" 2>/dev/null || true
sleep 1
mkfs.vfat -F 32 -n LAKKA "${L}p1"
mkfs.ext4 -q -L LAKKA_DISK "${L}p2"
mkdir -p /mnt/sd1
mount "${L}p1" /mnt/sd1
mkdir -p /mnt/sd1/res /mnt/sd1/joypads
cp "$KERNEL_IMG" /mnt/sd1/res/DATA01
cp "$SCRIPTBIN"  /mnt/sd1/res/DATA02
cp "$OUT/boot.scr" /mnt/sd1/boot.scr
[ -f "$RETROARCH" ] && cp "$RETROARCH" /mnt/sd1/retroarch
[ -f "$REPO/config/a13-retro-keys.cfg" ] && cp "$REPO/config/a13-retro-keys.cfg" /mnt/sd1/joypads/
echo "копирую SYSTEM..."
cp "$SYSTEM" /mnt/sd1/SYSTEM
sync
ls -la /mnt/sd1 /mnt/sd1/res
umount /mnt/sd1
losetup -d "$L"

gzip -c "$IMG" > "$IMG.gz"
echo "== готово =="
ls -la "$IMG" "$IMG.gz"
sha256sum "$IMG"
