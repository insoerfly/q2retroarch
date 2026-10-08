#!/bin/bash
# build_sdimage.sh — собрать ЗАГРУЖАЮЩИЙСЯ SD-образ для A13 (Q2/A13_4C).
#
# Проверенный рецепт: берём рабочий базовый образ (lakka-a13.img, p1 FAT с offset 4 МиБ),
# записываем в первые 4 МиБ boot_patch_4M.img (MBR+eGON.BT0+U-Boot SPL) и заменяем файлы на p1.
#
# ENV:
#   BASE      — базовый образ (по умолч. out/lakka-a13.img)
#   BOOTPATCH — boot_patch_4M.img (по умолч. bootloader/boot_patch_4M.img)
#   KERNEL_IMG — наш uImage (res/ext/DATA01)
#   SYSTEM     — squashfs SYSTEM
#   RETROARCH  — бинарь RetroArch
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
OUT=${OUT:-$REPO/out}
IMG=${IMG:-$OUT/lakka-full.img}
BASE=${BASE:-$OUT/lakka-a13.img}
BOOTPATCH=${BOOTPATCH:-$REPO/bootloader/boot_patch_4M.img}
KERNEL_IMG=${KERNEL_IMG:-$OUT/lakka-uImage}
SYSTEM=${SYSTEM:-$OUT/SYSTEM}
RETROARCH=${RETROARCH:-$OUT/retroarch}
SCRIPTBIN=${SCRIPTBIN:-$REPO/bootloader/script.bin}

for f in "$BASE" "$BOOTPATCH" "$KERNEL_IMG" "$SYSTEM" "$SCRIPTBIN"; do
  [ -f "$f" ] || { echo "НЕТ ФАЙЛА: $f"; exit 1; }
done

echo "== копирую базовый образ =="
cp -f "$BASE" "$IMG"

echo "== записываю загрузчик (первые 4 МиБ из boot_patch_4M.img) =="
dd if="$BOOTPATCH" of="$IMG" bs=512 count=8192 conv=notrunc status=none
sync

echo "== монтирую p1 (offset 4 МиБ) и обновляю файлы =="
MNT=/mnt/sdq2
mkdir -p "$MNT"
mount -o loop,offset=4194304,rw "$IMG" "$MNT"
mkdir -p "$MNT/res/ext" "$MNT/joypads"
cp -f "$KERNEL_IMG" "$MNT/res/ext/DATA01"
cp -f "$SCRIPTBIN"  "$MNT/res/ext/DATA02"
cp -f "$KERNEL_IMG" "$MNT/res/DATA01"
cp -f "$SCRIPTBIN"  "$MNT/res/DATA02"
[ -f "$RETROARCH" ] && cp -f "$RETROARCH" "$MNT/retroarch"
cp -f "$REPO/config/a13-retro-keys.cfg" "$MNT/joypads/a13-retro-keys.cfg"
cp -f "$SYSTEM" "$MNT/SYSTEM"
sync
ls -la "$MNT" "$MNT/res" "$MNT/res/ext"
umount "$MNT"

echo "== готово =="
ls -la "$IMG"
sha256sum "$IMG"
