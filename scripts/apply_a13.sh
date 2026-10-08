#!/bin/bash
# Apply "A13_4C" (Allwinner A13 / sun5i handheld) integration into the Lakka-LibreELEC tree.
set -e
REPO="${REPO:-$HOME/r3x/lakka}"
SRC="/mnt/c/distr/soft/r3xclone"
DEV="$REPO/projects/Allwinner/devices/A13_4C"
KCFG="$SRC/unpack/kernels/kernel_arm.config"
SCRIPTBIN="$SRC/res/DATA02"
MALI="$SRC/unpack/rootfs_arm/lib/libMali.so"
UMP="$SRC/unpack/rootfs_arm/lib/libUMP.so.3"

cd "$REPO"
mkdir -p "$DEV/linux/sunxi-3.4" "$DEV/bootloader/scripts" "$DEV/filesystem/usr/lib" "$DEV/patches/linux" "$DEV/packages"

echo "== 1. device options =="
cat > "$DEV/options" <<'EOF'
case $TARGET_ARCH in
  arm)
    TARGET_CPU="cortex-a8"
    TARGET_FLOAT="hard"
    TARGET_FPU="neon"
    ;;
esac

KERNEL_TARGET="zImage"

# Mali-400 (Utgard) GPU
MALI_FAMILY="400"

# use the Allwinner BSP 3.4 kernel (Linux/ARM), not mainline
LINUX="sunxi-3.4"

# mainline u-boot board mapping handled by scripts/uboot_helper
UBOOT_SYSTEM="a13-olinuxino"
UBOOT_TARGET=""
DEVICE_BOARDS="a13-olinuxino"

# firmware used by the vendored BSP audio/gpu paths
ADDITIONAL_PACKAGES=""
EOF

echo "== 2. kernel config (exact .config from the stock firmware) =="
cp "$KCFG" "$DEV/linux/sunxi-3.4/linux.arm.conf"

echo "== 3. board sys_config (script.bin) reused verbatim from DATA02 =="
cp "$SCRIPTBIN" "$DEV/bootloader/script.bin"

echo "== 4. Mali userspace blobs from the stock firmware =="
cp "$MALI" "$DEV/filesystem/usr/lib/libMali.so"
cp "$UMP"  "$DEV/filesystem/usr/lib/libUMP.so.3"
ln -sf libMali.so     "$DEV/filesystem/usr/lib/libEGL.so"
ln -sf libMali.so     "$DEV/filesystem/usr/lib/libGLESv2.so"
ln -sf libUMP.so.3    "$DEV/filesystem/usr/lib/libUMP.so"

echo "== 5. bootloader install (write script.bin + u-boot) =="
cat > "$DEV/bootloader/install" <<'EOF'
# SPDX-License-Identifier: GPL-2.0
# A13_4C: install u-boot-with-spl and the board script.bin
cp -av u-boot-sunxi-with-spl.bin $INSTALL/usr/share/bootloader/ 2>/dev/null || true
if [ -f "$PKG_DIR/script.bin" ]; then
  cp -av "$PKG_DIR/script.bin" $INSTALL/usr/share/bootloader/script.bin
fi
if [ -f "$PKG_DIR/scripts/boot.scr" ]; then
  cp -av "$PKG_DIR/scripts/boot.scr" $INSTALL/usr/share/bootloader/boot.scr
fi
EOF

echo "== 6. boot script for sunxi u-boot =="
cat > "$DEV/bootloader/scripts/boot.src" <<'EOF'
setenv bootargs "console=ttyS0,115200 loglevel=3 boot=LABEL=LAKKA disk=LABEL=LAKKA_DISK"
fatload mmc 0:1 ${kernel_addr_r} KERNEL
fatload mmc 0:1 ${fdt_addr_r} sun5i-a13-olinuxino.dtb
fatload mmc 0:1 0x43000000 script.bin
bootz ${kernel_addr_r} - ${fdt_addr_r}
EOF

echo "== 7. patch scripts/uboot_helper (add A13_4C board) =="
python3 - <<'PYEOF'
import re
p="scripts/uboot_helper"
s=open(p).read()
if "A13_4C" not in s:
    entry = """    'A13_4C': {
      'a13-olinuxino': {
        'dtb': 'sun5i-a13-olinuxino.dtb',
        'config': 'A13-OLinuXino_defconfig'
      },
    },
"""
    s=s.replace("  'Allwinner': {\n", "  'Allwinner': {\n"+entry,1)
    open(p,"w").write(s)
    print("patched uboot_helper")
else:
    print("uboot_helper already patched")
PYEOF

echo "== 7b. download BSP kernel source and compute sha256 =="
mkdir -p sources
KV="d47d367036be38c5180632ec8a3ad169a4593a88"
TB="sources/linux-sunxi-3.4-$KV.tar.gz"
if [ ! -f "$TB" ]; then
  curl -L --retry 3 -o "$TB" "https://github.com/linux-sunxi/linux-sunxi/archive/$KV.tar.gz"
fi
SHA=$(sha256sum "$TB" | cut -d' ' -f1)
echo "kernel tarball: $TB  sha256=$SHA"

echo "== 8. patch packages/linux/package.mk (add sunxi-3.4 source) =="
export SHA
python3 - <<'PYEOF'
import os
p="packages/linux/package.mk"
s=open(p).read()
if "sunxi-3.4)" not in s:
    case = """  sunxi-3.4)
    PKG_VERSION="d47d367036be38c5180632ec8a3ad169a4593a88"
    PKG_SHA256="%s"
    PKG_URL="https://github.com/linux-sunxi/linux-sunxi/archive/$PKG_VERSION.tar.gz"
    PKG_SOURCE_NAME="linux-$LINUX-$PKG_VERSION.tar.gz"
    PKG_PATCH_DIRS=""
    ;;
""" % os.environ["SHA"]
    s=s.replace("  L4T)\n", case+"  L4T)\n",1)
    open(p,"w").write(s)
    print("patched package.mk")
else:
    print("package.mk already patched")
PYEOF

echo "APPLY DONE"
