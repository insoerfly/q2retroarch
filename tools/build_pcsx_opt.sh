#!/bin/bash
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC=$B/toolchain
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
P=$B/pcsx_rearmed-aced3eb
H=$P/libpcsxcore/new_dynarec/assem_arm.h
LOG=/root/pcsx_vendor.log
exec > "$LOG" 2>&1
echo "START $(date)"
sed -i 's/#define TARGET_SIZE_2 23/#define TARGET_SIZE_2 24/' "$H"
grep -m1 "TARGET_SIZE_2" "$H"
cd "$P"
make -f Makefile.libretro clean >/dev/null 2>&1 || true
rm -f pcsx_rearmed_libretro.so
echo "=== build (default flags) ==="
make -f Makefile.libretro \
  CC="$CROSS""gcc" CXX="$CROSS""g++" AR="$CROSS""ar" \
  HAVE_NEON_ASM=1 DYNAREC=ari64 ARCH=arm BUILTIN_GPU=neon -j"$(nproc)" 2>&1 | tee /tmp/pcsx_make.log | tail -3
echo "MAKE RC=${PIPESTATUS[0]}"
echo "=== sample compile flags ==="
grep -m2 -oE "gcc .*-c .*assem_arm.*|gcc .*new_dynarec.*-c" /tmp/pcsx_make.log | head
grep -m1 -oE "\-O[0-3s][^ ]*|\-mtune=[^ ]*|\-ffast-math|\-mcpu=[^ ]*|\-march=[^ ]*" /tmp/pcsx_make.log
echo "--- all -O/-mtune occurrences (counts) ---"
grep -oE "\-O[0-3s]|\-mtune=[^ ]*|\-mcpu=[^ ]*" /tmp/pcsx_make.log | sort | uniq -c
ls -la pcsx_rearmed_libretro.so; md5sum pcsx_rearmed_libretro.so
echo "DONE $(date)"
