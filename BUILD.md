# BUILD.md — пошаговая сборка

Предполагается дерево Lakka-LibreELEC (`PROJECT=Allwinner DEVICE=A13_4C ARCH=arm`).
Здесь описаны ручные шаги и что именно пропатчено.

## 0. Патчи поверх апстрима

### 0.1 Ядро (`linux-sunxi` 3.4, commit `d47d367036be38c5180632ec8a3ad169a4593a88`)
- `drivers/video/sunxi/disp/dev_fb.c` → **A13FORCE**: `init_para->format[0]=DISP_FORMAT_ARGB8888` (32bpp),
  `init_para->buffer_num[0]=3` (3 буфера).
- `drivers/video/sunxi/lcd/lcd0_panel_cfg.c` → панель GC9307; в `a13_backlight_on()` **убраны** строки,
  гоняющие `PE0..PE11` и `PB16` (это кнопки): оставлены только пины подсветки (PC/PG/PB10/2/4/3/15).
- `drivers/input/a13keys.c` → новый драйвер кнопок (см. `kernel/`). Собирается как встроенный:
  добавить `obj-y += a13keys.o` в `drivers/input/Makefile`.
- `.config` (`kernel/kernel.config`): `CONFIG_INPUT_EVDEV=y`, `CONFIG_INPUT_JOYDEV=y`,
  `CONFIG_STRICT_DEVMEM` **выключен**, `CONFIG_FB_SUNXI=y`, squashfs/overlay/zram, FRAMEBUFFER_CONSOLE,
  `CONFIG_CMDLINE="console=ttyS0,115200 console=tty0 loglevel=7 rdinit=/a13init"`.

### 0.2 RetroArch (`retroarch-ad89b0c`, v1.14.0)
- `gfx/drivers/sunxi_gfx.c` (`retroarch/`) — наш видеодрайвер софт-рендера.
- `config.mk` (`retroarch/`): `HAVE_SUNXI = 1` и `-DHAVE_SUNXI` в `CFLAGS`/`CXXFLAGS`.
  `Makefile.common` добавляет `sunxi_gfx.o`, но НЕ определяет `HAVE_SUNXI` — поэтому обязательно в config.mk.
- Патч `sunxi_set_texture_enable()`: **не** делать `sthread_join(vsync_thread)` (дедлок меню).
- Путь меню: убран `ioctl(FBIO_WAITFORVSYNC)` (LCD даёт vsync ~354 мс → меню тормозит).

## 1. Ядро + initramfs

Тулчейн: `build.Lakka-A13_4C.arm-3.7.5-devel/toolchain/bin/armv7a-libreelec-linux-gnueabi-`.

```sh
cd <linux-src>
cp kernel/a13keys.c drivers/input/
grep -q a13keys drivers/input/Makefile || echo "obj-y += a13keys.o" >> drivers/input/Makefile
cp kernel/a13init <initramfs-dir>/a13init         # наш init (см. initramfs/a13init)
rm -f usr/initramfs_data.cpio.gz usr/initramfs_data.o
make ARCH=arm CROSS_COMPILE=<tc>/bin/armv7a-libreelec-linux-gnueabi- \
     HOSTCC=<tc>/bin/host-gcc HOSTCXX=<tc>/bin/host-g++ \
     HOSTCFLAGS="<...>" KCFLAGS="-fgnu89-inline" -j$(nproc) zImage
<u-boot>/tools/mkimage -A arm -O linux -T kernel -C none -a 0x40008000 -e 0x40008000 \
     -n "A13_4C Lakka" -d arch/arm/boot/zImage out/lakka-uImage
```

GCC-9 фиксы (если тулчейн GCC9): `include/linux/compiler-gcc9.h`, `__asmeq(x,y) ""`.

### 1.1 Пересборка busybox initramfs с `mkfs.vfat`
init использует `mkfs.vfat` при первом расширении раздела. В init‑busybox включить:
`CONFIG_MKFS_VFAT=y` (и, при желании, `CONFIG_MKDOSFS=y`) в
`busybox-1.31.0/.<target>-init/.config`, затем пересобрать (`make busybox:init` через основной билд системы
или вручную в каталоге сборки busybox) и установить бинарь в initramfs.

## 2. RetroArch

```sh
cd <retroarch-src>
cp ../retroarch/sunxi_gfx.c gfx/drivers/
# config.mk: HAVE_SUNXI=1, -DHAVE_SUNXI в CFLAGS/CXXFLAGS
make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1 HAVE_FREETYPE=1 -j$(nproc)
cp retroarch out/retroarch-sunxi
```

## 3. SD-образ

```sh
sudo scripts/build_sdimage.sh    # соберёт out/lakka-full.img (+ .gz)
```

Сборщик:
1. делает образ 2600 МБ, размечает: p1 FAT32 (1.5 ГБ, boot), p2 ext4;
2. копирует загрузчик Allwinner из `bootloader/boot0-boot1_8k-1mb.bin` в секторы 16..2047;
3. кладёт на p1: `res/DATA01` (наш uImage), `res/DATA02` (script.bin), `boot.scr`,
   `retroarch`, `joypads/a13-retro-keys.cfg`, `SYSTEM`.

Референсный `20240628.img` (вендорский) нужен только как источник загрузчика — он уже вынесен в
`bootloader/boot0-boot1_8k-1mb.bin`.

## 4. Первый запуск
Init (`initramfs/a13init`) при первом старте: если нет большого FAT‑раздела — создаёт `p3` строго
**после p2** (секторы `p2_start+p2_size+4096..end`), перезагружается, форматирует `p3` (`mkfs.vfat`)
и монтирует как `/storage`. Дальше — RetroArch; настройки пишутся в
`/storage/.config/retroarch/retroarch.cfg`.

## 5. Полезное
- Логи на FAT: `RA.LOG` (RetroArch), `RETRO.LOG` (init), `DMESG.txt`.
- Кнопки: автоконфиг `joypads/a13-retro-keys.cfg` (js-индексы) + прямые биндинги в init.
- Раскладка Q2 (проверена эмпирически): стик PE11/PE10/PE9/PE8 = ↑/↓/←/→,
  A=PE2, B=PE4, X=PE3, Y=PE5, L=PE7, R=PE6, Start=PE0, Select=PB16, Menu=PE1.
