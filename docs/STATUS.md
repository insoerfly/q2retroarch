# Lakka для Allwinner A13 (R3X / плата `A13_4C`) — текущий статус

Обновлено: 2026-10-06

## Кратко

- **Собран полноценный образ Lakka с RetroArch** (`lakka-a13/out/`).
  Сборочная система **Lakka-LibreELEC v3.x**, **родное BSP-ядро sunxi 3.4.104**
  с `.config`, вытащенным из стоковой прошивки, её `script.bin` и Mali-блобы.
- **RetroArch В ОБРАЗЕ** (`MEDIACENTER="retroarch"`), вместе с ядрами libretro
  (fceumm, nestopia, snes9x2002/2005/2010, gambatte, mgba, genesis-plus-gx,
  picodrive, prosystem, stella, vecx, gme, handy, pcsx_rearmed, mame2003-plus),
  ассетами/overlay, базой ядер, glsl-шейдерами и midi (`libretro-cores` — 189/189).
- **Исправлена загрузка (была причина «чёрного экрана»).** Раньше образ не содержал
  u-boot (на смещении 8 КБ были нули), а BSP-ядро не получало `script.bin`
  (шёл mainline-`extlinux` с `video=HDMI…` и отсутствующим DTB). Теперь в образ
  пишется `u-boot-sunxi-with-spl.bin` на 8 КБ, а `boot.scr` грузит `KERNEL` и
  `script.bin` в `0x43000000` и делает `bootz` (см. «Загрузка (boot chain)»).
- **Карта правится на месте** (без полной перепрошивки): FAT-раздел уже поправлен,
  а u-boot дописывается маленьким патчем `out/boot_patch_4M.img` (4 МиБ) через
  Win32 Disk Imager — см. «Патч microSD на месте».
- Из-за VPN на Windows у WSL2 отвалилась сеть; загрузка исходников сделана через
  **мост в Windows** (`curl.exe` / `git.exe`) — см. «Сеть».

## Железо (из прошивки, `unpack/REPORT.md`)

- SoC: **Allwinner A13 / sun5i**, CPU **Cortex-A8**, GPU **Mali-400**.
- Плата: **`A13_4C`** (board-config на базе Olimex A13-OLinuXino-MICRO).
- Ядро стока: `Linux 3.4.104-gee852210-dirty` (`a13@ub`, Linaro GCC 4.9.4),
  `ARCH_SUN5I`, `FB_SUNXI_LCD` (RGB) + `UMP`, `MALI400/UMP=m`,
  `SND_SUNXI_SOC_CODEC`, cmdline `console=ttyS0,115200 loglevel=3`.

## Что сделано

### 1. Интеграция устройства в Lakka (`projects/Allwinner/devices/A13_4C`)
- `options`: `cortex-a8/hard/neon`, `KERNEL_TARGET=zImage`, `MALI_FAMILY=400`,
  `LINUX=sunxi-3.4`, `UBOOT_SYSTEM=a13-olinuxino`, `DEVICE_BOARDS=a13-olinuxino`,
  отключены лишние фичи (samba/nfs/avahi/bt/nano/…), урезан список ядер.
- `linux/sunxi-3.4/linux.arm.conf` — **точный конфиг ядра из прошивки (IKCONFIG)**.
- `bootloader/script.bin` — **board-config из `res/DATA02`** (переиспользован).
- `filesystem/usr/lib/libMali.so`, `libUMP.so.3` (+симлинки EGL/GLESv2) —
  **GPU-блобы из прошивки**.
- `scripts/uboot_helper` — добавлен борд `a13-olinuxino`
  (`A13-OLinuXino_defconfig`, dtb `sun5i-a13-olinuxino.dtb`).
- `packages/linux/package.mk` — добавлен источник `sunxi-3.4`
  (`github.com/linux-sunxi/linux-sunxi`, commit `d47d3670…`).

Скрипт-накат: `lakka-a13/apply_a13.sh`.

### 2. Сборка

- `PROJECT=Allwinner DEVICE=A13_4C ARCH=arm make image` **завершилась успешно**.
- Артефакты: `lakka-a13/out/` (актуальная сборка от 2026‑10‑06 **23:19**):
  - `Lakka-A13_4C.arm-3.7.5-devel-20261006231944-bbfdbb7-a13-olinuxino.img.gz`
    (≈567 МБ, sha256 `…`), плюс распакованный `.img` (2,04 ГБ) для Win32 Disk Imager;
  - `…-a13-olinuxino.tar` (540 МБ), `.kernel` (4,4 МБ), `.system` (536 МБ).
  - Ранее: образ 22:47 — **с RetroArch, но без u-boot/script.bin** (не грузился);
    образ 20:23 (36 МБ) — базовая проверка сборки без RetroArch.
- Объём вырос из-за ассетов/overlay (`retroarch-assets`/`retroarch-overlays`),
  `libretro-database` и `glsl-shaders`; при желании их можно урезать.
- В образе: ядро A13 (`zImage`), rootfs (squashfs), RetroArch + меню Lakka,
  ядра libretro, busybox, systemd, connman, ALSA, **Mali-блобы**
  (`/usr/lib/libMali.so`, `libUMP.so`, `libEGL.so`, `libGLESv2.so`).

## RetroArch — собран

`MEDIACENTER="retroarch"` включён → список сборки стал **189** шагов. Успешно собраны:
`retroarch` (184 МБ исходников), `mediacenter`, `retroarch-assets/overlays`,
`core-info`, `libretro-database`, `glsl-shaders`, `libretro-joypad-autoconfig`,
`lakka-update`, `ffmpeg` (Leia 4.0.4), `libass`, `libxkbcommon`, `joyutils`,
`libmali`, ядра `libretro-cores` (fceumm, nestopia, snes9x2002/2005/2010, gambatte,
mgba, genesis-plus-gx, picodrive, prosystem, stella, vecx, gme, handy, pcsx_rearmed,
mame2003-plus). Итог: `[189/189] install image` → «Successful build».

## Загрузка (boot chain) — исправлено

Первое устройство дало «чёрный экран без подсветки». Диагностика:

- В образе на смещении **8 КБ были нули** — `u-boot` вообще не записан (BROM нечего
  грузить → нет старта). Причина: у устройства A13_4C не было
  `projects/Allwinner/devices/A13_4C/bootloader/release`, поэтому
  `u-boot-sunxi-with-spl.bin` не попадал в `target/<img>/3rdparty/bootloader`,
  откуда его берёт `scripts/mkimage` (у H3/A64/H6 такой `release` есть).
- Ядро — **BSP sunxi-3.4 без device tree** (`CONFIG_USE_OF` off), читает
  **`script.bin`** из фиксированного адреса **`0x43000000`**
  (`SYS_CONFIG_MEMBASE = PLAT_PHYS_OFFSET + 48M`, `arch/arm/plat-sunxi/core.c`).
  Но образ собирался через mainline-**`extlinux`** с `FDT /sun5i-a13-olinuxino.dtb`
  (файла нет) и `video=HDMI-A-1:1280x720@60` — ни `script.bin`, ни u-boot в образе,
  отсюда «чёрный экран».

Что сделано:
- `bootloader/release` (A13_4C): кладёт `u-boot-sunxi-with-spl.bin`, `script.bin`
  и скомпилированный `boot.scr` в `$RELEASE_DIR/3rdparty/bootloader`.
- `bootloader/mkimage` (A13_4C): пишет u-boot на 8 КБ, удаляет `extlinux` и
  добавляет в FAT-раздел `script.bin` + `boot.scr`.
- `bootloader/scripts/boot.src`: `bootz` без FDT, `bootargs` без `video=HDMI…`.
- `options`: `EXTRA_CMDLINE` без HDMI.
- Проверено на образе: на 8 КБ — `eGON.BT0`/`U-Boot SPL 2019.04`; в FAT-разделе
  `KERNEL`, `SYSTEM`, `script.bin`, `boot.scr`, без `extlinux`.

Порядок загрузки: BROM → SPL (`u-boot-sunxi-with-spl.bin` @8K) → U-Boot →
`boot.scr` (`fatload KERNEL 0x42000000`, `fatload script.bin 0x43000000`) → `bootz`
→ BSP-ядро читает `script.bin` (LCD/подсветка) → initramfs монтирует `SYSTEM`.

## Патч microSD на месте (без полной перепрошивки)

После первой прошивки карту можно доделать без записи всего образа. Карта:
**30 ГБ, `Generic STORAGE DEVICE`, PhysicalDrive1**; тома `G: LAKKA` (FAT32) и
`E: LAKKA_DISK` (ext4). Нужно ровно два изменения:

- **FAT-раздел `G:` — правится без прав администратора:** удалён `extlinux`,
  добавлены `script.bin` и `boot.scr`. (Сделано.)
- **u-boot на смещении 8 КБ — нужен админ.** Подготовлен `out/boot_patch_4M.img`
  — первые 4 МиБ рабочего образа (MBR + `eGON.BT0`/`U-Boot SPL`), пишется поверх
  начала SD; раздел `LAKKA` начинается ровно с 4 МиБ, поэтому не затрагивается.

Ограничения среды (почему u-boot не удалось записать программно из-под агента):
- сырая запись в `\\.\PhysicalDriveN` требует прав администратора и блокируется,
  пока на диске есть примонтированные тома; `E:` (ext4) Windows не размонтирует;
- `FSCTL_LOCK_VOLUME`/`FSCTL_DISMOUNT_VOLUME` из PowerShell и `mountvol /p` не
  снимают блокировку полностью (UAC выданы, но запись всё равно отклонена);
- `wsl --mount \\.\PHYSICALDRIVE1 --bare` этот USB-картридер не поддерживает
  (`ERROR_INVALID_DRIVE`).

Поэтому u-boot записывается **Win32 Disk Imager** (`C:\Program Files (x86)\ImageWriter\Win32DiskImager.exe`)
образом `out/boot_patch_4M.img`. Полный образ (`out/…20261006231944….img`) —
запасной путь.

Файлы для ручной правки в `out/`: `u-boot-sunxi-with-spl.bin` (478 624 Б,
sha256 `fd0ae84f…`), `script.bin` (30 156 Б), `boot.scr` (263 Б), `boot_patch_4M.img`.

## Ключевые исправления и обходы (иначе на Ubuntu 24.04 не собирается)

Среда сборки: Windows 10 + **WSL2 Ubuntu 24.04**, host glibc 2.39 / GCC 13; сборка —
от пользователя `agent` (root запрещён).

Прошивка/ядро (BIOS/BSP 3.4 + новые toolchain):
- `make` (4.2.1): `__stat` → `stat` (glibc 2.39).
- `m4` (1.4.18): `SIGSTKSZ` стал runtime-выражением → forced `#define SIGSTKSZ 16384`.
- **host Python 3.7 + GCC13**: выровненный `vmovdqa` на невыровненном объекте →
  SIGSEGV в `_ctypes`. Лечение: `-fno-tree-slp-vectorize -fno-tree-vectorize`
  в host‑CFLAGS (`config/optimize`). Также добавлен `libffi` в target-зависимости
  Python3 и обход `setuptools` (guarded `import ctypes`).
- Ядро 3.4 нет цели `olddefconfig` → в рецепте заменено на `oldnoconfig`.
- Ядро 3.4 без `compiler-gcc9.h` → симлинки `compiler-gcc{5..15}.h` → `compiler-gcc4.h`.
- Ядро 3.4 `__asmeq(... ".err" ...)` не совместим с GCC9 → макрос обнулён.
- Ядро 3.4 требует `linux/btrfs.h`, `linux/prctl.h` и новые netlink-UAPI →
  скопированы свежие UAPI-заголовки в sysroot; `asm/sockios.h` дополнен `*_OLD`.
- `nss` 3.58: `-Werror=enum-int-mismatch` (GCC9) → `NSS_ENABLE_WERROR=0`.
- `perf` (linux tools) не собирается с glibc 2.34+ (`libio.h`) → положена заглушка
  `libio.h`; `PKG_BUILD_PERF="no"`.
- Проектные патчи Allwinner для mainline (sun4i-i2s, hdmi-codec, H3-u-boot)
  отключены — они не для BSP 3.4.

Пакеты RetroArch:
- `libxcb` 1.13: install‑relink libtool падал на `libxcb-xkb.so.1.0.0` в staged
  sysroot. В `package.mk` добавлен `pre_makeinstall_target`, убирающий
  `relink_command` из `.la` перед установкой.
- `mali-utgard` (внешний kernel‑драйвер) не собирается под ядро 3.4
  (`ktime_get_boottime`, `.tv64`, `__setup_timer`). BSP‑ядро уже даёт Mali
  (`CONFIG_MALI=m`, `CONFIG_MALI400=m`, `CONFIG_UMP=m`), поэтому пакет исключён:
  `sunxi-3.4` добавлен в исключения в `packages/graphics/libmali/package.mk`.
- `picodrive` требует git‑сабмодули (`cpu/cyclone` и др.) — теперь тянутся
  рекурсивно (см. «Сеть»); после засева пакет принудительно пере‑распаковывается.

## Стоковый образ `20240628.img` и вариант A (загрузка как у стока)

В корне найден **полный образ флешки стока** `C:\distr\soft\r3xclone\20240628.img`
(418 МБ, файл обрезан; карта ~64 ГБ, один FAT32-раздел). В нём:
- на смещении 8 КБ — **штатный загрузчик**: `eGON.BT0` + U-Boot SPL (26.03.2024) +
  **Allwinner BSP U-Boot 2017.03** (верный DRAM + handoff `script.bin`);
- FAT-раздел: `res/` (`DATA01` = ядро uImage, `DATA02` = `script.bin`, `DATA03` = 274 МБ
  контейнер, `system.img`), `roms/`, `save/`, `download/`;
- ядро стока имеет **вшитый initramfs** (rootfs внутри ядра).
- Флешка стока грузится (проверено пользователем).

Штатный boot-flow: `fatload mmc 0:1 0x42000000 res/DATA01` + `fatload mmc 0:1
0x43000000 res/DATA02` + `bootm 0x42000000`; `bootargs=console=ttyS3,115200 loglevel=2`
(`bootcmd=run mmcboot`).

Важно: наше ядро собрано из вендорского `.config` с **`CONFIG_CMDLINE_FORCE=y`** —
оно **игнорирует cmdline загрузчика** и не получало `boot=` (init без `boot=` не
находит SYSTEM — это объясняет и прошлые неудачи). Поэтому `CONFIG_CMDLINE` устройства
изменён на `console=tty0 console=ttyS0,115200 loglevel=7 boot=LABEL=LAKKA
disk=LABEL=LAKKA_DISK`, ядро пересобрано.

**Тестовый образ (вариант A)**: `out/lakka-sd.img` (2.5 ГБ, sha256 `f7dbb852…`):
- стоковый загрузчик (байты 8 КБ..1 МБ из `20240628.img`);
- p1 FAT32 `LAKKA`: `res/DATA01` = наше ядро (uImage), `res/DATA02` = `script.bin`,
  `SYSTEM` = squashfs, `boot.scr`;
- p2 ext4 `LAKKA_DISK` (хранилище).
Вспомогательные файлы: `out/kernel.uImage`, `out/boot.scr`.

### Вариант A — не сработал

Тестовый образ на основе нашего ядра (`out/lakka-sd.img`, sha `f7dbb852…`) на железе
**не поднялся** (нет изображения/подсветки). Вероятно, community‑ядро не инициализирует
LCD этой платы так, как вендорское.

### Вариант B (стоковое ядро + наш rootfs)

Чтобы гарантировать железо, взято **стоковое ядро** и к нему подложен наш rootfs:
- `res/DATA01` стока = zImage `Linux-3.4.104-gee852210-dirty` (load 0x40008000) —
  оно точно работает с LCD/звуком платы;
- собран **multi‑image uImage** = стоковое ядро + **наш LibreELEC‑initramfs**
  (`mkimage -T multi -d stock_zImage:initramfs.cpio.gz`) → `out/lakka_stock_uImage`;
  в initramfs‑`init` добавлены дефолты `boot=LABEL=LAKKA`/`disk=LABEL=LAKKA_DISK`;
- стоковый u-boot грузит его как `res/DATA01` (`bootm`); kernel распаковывает
  встроенный initramfs, затем **наш** (наш `/init` перекрывает стоковый), монтирует
  наш `SYSTEM` и делает `switch_root` в Lakka.
- Тестовый образ: `out/lakka-sd.img` (sha256 `207d3bf3…`).
Вспомогательные файлы: `out/stock_zImage`, `out/initramfs.cpio.gz`, `out/lakka_stock_uImage`.

### Вариант B — тоже не сработал

Образ `out/lakka-sd.img` (sha `207d3bf3…`: стоковое ядро + наш initramfs) на железе
**не поднялся** (нет изображения/подсветки). Вероятные причины: стоковый u-boot/ядро
не подхватывает внешний initrd, либо `CONFIG_CMDLINE_FORCE` у стокового ядра, либо
несовместимость нашего userspace со старым ядром 3.4.

### Выбран путь 1 (нативный): оставить сток, поставить RetroArch

Решение: сохранить **всю стоковую систему** (загрузчик, ядро с встроенным rootfs,
MiniGUI, ALSA, дисплей, инпут, ресурсный раздел) и вместо стоковых эмуляторов
запускать **RetroArch, собранный под стоковый ABI**. Правка самого initramfs ядра не
требуется: меняем только **незашифрованные** части (ресурсный раздел `DATA03`).

Что выяснено (шаг 1 — разбор rootfs/ресурсов стока):

- **Стоковый ABI (точный):** ARM 32‑bit, EABI5, hard‑float, интерпретатор
  `/lib/ld-linux-armhf.so.3`; libc — **eglibc/glibc 2.19**
  (`libc-2.19-2014.08-1-git.so`, Linaro eglibc 2_19). Бинарники собраны тулчейном
  **`gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf`**, GNU C **4.9.4**, флаги
  `-march=armv7-a -mtune=cortex-a9 -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb`.
  → Нужен ровно этот кросс‑тулчейн (Linaro 4.9.4‑2017.01) и sysroot на стоковых `.so`.
- **`DATA03` (`unpack/rootfs_arm/`) — не rootfs, а ресурсный/прикладной раздел,
  незашифрован:** `bin/` (тесты `video_test*`, `mali_test`, `emu_*_test`, `amixer`,
  `aplay`, `memtester`), `lib/` (плагины эмуляторов `libemul*.so`, `libMali.so`,
  `libEGL/GLESv2`, `libUMP`, ALSA, ffmpeg 56/54, MiniGUI SA 3.0, freetype, png12,
  jpeg8, xml2, zlib, usb‑1.0, drm, `libstdc++.so.6.0.20`), `ui_*` (скины MiniGUI),
  `xml*` (языки), `db/` (SQLite‑списки игр), `wav/`, `font/`, `lost+found/`.
  `/etc`, `/sbin`, `/init` отсутствуют.
- **Архитектура эмуляторов — «тонкий лаунчер + плагин‑ядро»:** системный бинарник
  (`bin/emu_nes_test`) импортирует `libsys_init`, `system_audio_init`,
  `keypad_set_game_runing_state` и **экспорты ядра** (`nes_init/nes_run/nes_reset/
  nes_continue/nes_exit`), которые даёт `libemulnes.so`; рендер — Mali/EGL, звук —
  ALSA, обёртка — `libsys_a13_a.so` (`sys_get_emu_type`/`sys_set_emu_type`,
  `system_render_*`, `system_audio_*`, вызов `system()` для `amixer`).
- **Меню/лаунчер — зашифрован** во встроенном в ядро initramfs (`usr/data/bin/entry_4c`
  — лаунчер модели 4C, `entry_Q2`, `entry_A`, `init`, `do_mount_res`, `inittab`).
  Содержимое записей зашифровано (энтропия ≈ 7.99, единый заголовок
  `…01 23 45 67 89 AB CD EF`); штатный `cpio`/`7z` не извлекают (`Malformed number`).
  **Правка лаунчера невозможна без ключа вендора.**
- **Список игр:** SQLite `db/game_list_a13_4c_64g.db` (SQLite 3.35), таблица
  `tbl_all(game_id, en_name, cn_name, cn_match, suffix CHAR(8), class_type INTEGER,
  emu_type INTEGER, img_name CHAR(64), long_en_name CHAR(168))` — поле `emu_type`
  выбирает плагин‑эмулятор. `res/DATA04` — тоже SQLite (служебная БД 8 КБ).
- **Прочее в `res/`:** `DATA01` = стоковое ядро‑uImage `Linux-3.4.104-gee852210-dirty`
  (load 0x40008000), `DATA02` = `script.bin`, `DATA06` = 128 МБ раздел; `res/ext/` —
  вторая копия ядра `Linux-3.4.104-…` + `settings/`; `res/lib/` `libSDL.so` (28 КБ) и
  `libunity.so` (4.8 МБ); `res/settings/{setting,factory}.txt` — конфиг (язык,
  LCD‑таймаут, маппинг кнопок); `res/system.img` — посторонний x86.

Следствие: **правка встроенного rootfs ядра заблокирована** (шифрование), но
прикладной слой (`DATA03`) открыт и ABI известен. Значит «нативный вариант 1»
реализуем **без правки ядра**: собираем RetroArch (или libretro‑фронтенд) под
eglibc 2.19 / Linaro 4.9.4 и подменяем/подсовываем запуск. Способы запуска (по
убыванию простоты):
1. **Шим‑подмена плагина:** заменить один `libemul*.so` (или `bin/emu_*_test`) на
   свой, который делает `execve` RetroArch с нужным ядром — тогда выбор системы в
   стоковом меню запускает RetroArch. Нужно точно воспроизвести интерфейс плагина.
2. **Хук автозапуска:** найти в открытых `settings/`/БД точку, откуда можно
   стартовать свой бинарник до/вместо меню.

План:
1. Поставить кросс‑тулчейн **Linaro 4.9.4‑2017.01** (arm-linux-gnueabihf) + sysroot
   из стоковых `.so`, синхронный с eglibc 2.19.
2. Собрать RetroArch + ядра libretro + зависимости (zlib, ALSA, EGL/Mali, fbdev) под
   этот ABI.
3. Определить точный интерфейс плагина (`nes_*`/`libsys_*`/`emu_*`) и сделать шим.
4. Положить в ресурсный раздел/на FAT, запустить на железе, проверить.

Статус: **шаг 1 завершён** (ABI, архитектура и границы известны); далее — bring‑up
своего ядра (см. ниже).

## Диагностический bring-up: своё ядро + диагностический initramfs

Стратегия «первого света»: собрать **своё BSP‑ядро** (linux-sunxi `linux-3.4.104`, тот
же коммит `d47d367`, что у вендора) с fbcon и крошечным диагностическим `/init`, и
грузить его **стоковым U‑Boot** (uImage кладётся как `res/DATA01` и `res/ext/DATA01`,
`res/DATA02` = `script.bin`, `res/system.img` оставляем — U‑Boot проверяет его наличие).
Образ собирается **на базе оригинала** `20240628.img` (полный `res/`), раздел под карту
(~2 ГБ), U‑Boot на секторе 16 (`cfdisk`/`sfdisk`; Windows‑монтирование ломается, если
раздел больше карты — поэтому размер под карту, а не от 64‑ГБ дампа).

Сборка ядра — напрямую в дереве `build…/linux-d47d…` тулчейном
`armv7a-libreelec-linux-gnueabi-` (GCC 9.4), **обязателен `KCFLAGS="-fgnu89-inline"`**
(иначе `multiple definition of 'return_address'`); `mkimage -a 0x40008000` → uImage.
Диагностический `init` — статический ARM‑бинарник (`armv7a-…-gcc -static`), пишет
телеметрию на SD в `DIAG.TXT` (монтирует `/dev/mmcblk0p1`, vfat): uname, cmdline,
`/proc/fb`, `/dev/fb0`, dmesg (`klogctl`), опрос `/dev` и `/sys/class`, тесты GPIO.

Что найдено/исправлено (итерации diag1..diag27):

1. **Чёрный экран из‑за U‑Boot:** стоковый U‑Boot грузит ядро из **`res/DATA01`** +
   `res/DATA02` (script.bin) и **проверяет наличие `res/system.img`** (содержимое не
   важно). Минимальный образ без `system.img` → загрузка срывалась (минимальный образ
   `diag-sd.img` не грузился именно поэтому). Также вендорский env по умолчанию грузит
   `res/system.img`+`script.bin`, а сохранённый/сборочный — `res/DATA01`+`res/DATA02`.
2. **Cmdline:** у ядра `CONFIG_CMDLINE_FORCE=y` — cmdline форсируется, bootargs U‑Boot
   игнорируются. Включены `CONFIG_FRAMEBUFFER_CONSOLE=y`, `CONFIG_LOGO`,
   `CONFIG_FONT_8x16`; cmdline `console=tty0 console=ttyS0,115200 loglevel=8`;
   вшит наш initramfs (`CONFIG_INITRAMFS_SOURCE=<diagroot>`).
3. **Резерв памяти под LCD — главный баг:** при `CONFIG_CMA=y` в
   `arch/arm/plat-sunxi/core.c` `reserved_start = bank.start + SZ_256M`, а RAM ровно
   256 МиБ → `Not enough memory to reserve memory for LCD` → `fb_start=0, fb_size=0` →
   disp создаёт heap по нулевому адресу → картинки нет. **Пропатчено: `SZ_256M`→`SZ_64M`**
   (вендор, судя по всему, сделал так же). После — в логе `LCD : 0x44000000 - 0x45ffffff
   (32 MB)`, `fb0 = 800x480 bpp=16`, `Console: switching to colour frame buffer device`.
4. **Тайминги панели:** в стоковом `script.bin` секция `lcd0_para` содержит только
   `lcd_type=3`, `lcd_used`, `lcd_pwm_not_used` и распиновку — **таймингов нет**;
   вендорское ядро хардкодит их (тред 4PDA). В нашем `lcd0_panel_cfg.c` было
   `//#define LCD_PARA_USE_CONFIG` → таймингов нет вовсе. Включён `LCD_PARA_USE_CONFIG`,
   значения приведены к драйверу панели `hv_800x480_td043`: 800×480, dclk 33 МГц,
   ht 1056, hbp 216, hspw 10, vt 1050, vbp 35, vspw 10, pwm 12500.
5. **Подсветка (разобрано по стоковым файлам):** в FEX `lcd0_para` нет `lcd_bl_en`
   (`lcd_pwm_not_used=1`), поэтому дисплейное ядро подсветку не включает. В стоке она
   управляется из userspace: `libsys_a13_a.so` → `ioctl(fd, 0x40046c04)` =
   `SET_LCD_LED_STATE` на вендорском `/dev/driver_misc` (в нашем ядре его НЕТ), причём
   `sys_lcd_led` **инвертирует** аргумент; fallback‑путь пишет `echo 0 >
   /sys/class/gpio/gpio14_pb10/value`, где **`gpio14` = PB10**. В ядре вендора есть
   `BSP_disp_close_lcd_backlight`, `open_lcd_backlight`, `lcd_bl_en`,
   `lcd%d_backlight`, `board_led1`, `GPIO_LCD_LED1` (`lcd_led1`), `driver_misc`.
6. **Точная расшифровка FEX `gpio_para`** (в FEX порты кодируются A=1…H=8): 30 пинов.
   Ключевое — **выходами (`mul=1`) сделаны ровно два пина, оба со стартовым `data=0`**:
   - `gpio_pin_14` = **PB10** (mul=1, data=0)
   - `gpio_pin_18` = **PG9** (mul=1, data=0)
   все остальные 28 — входы (`mul=0`). Порядок пинов:
   `1..12=PE0..PE11, 13=PB16, 14=PB10, 15=PG1, 16=PB2, 17=PB4, 18=PG9, 19..21=PG10..PG12,
   22=PC10, 23=PB3, 24=PB15, 25=PC14, 26=PC15, 27..30=PC0..PC3`.
   В `leds_para` PG9 = `green:pg09:led1`, но `leds_used=0` (отключён).
   Строки в `libsys` `PB16/PB17/PB18` и `PE00..PE11` — это keypad‑пины, не подсветка.
7. **Эмпирика по итерациям (diag9..diag27):**
   - `diag9/10`: мигание всеми 30 пинами `gpio_para` → подсветка мигает;
   - `diag12`: по‑пиновые «вспышки» (PB‑first) — завис на `PC0`;
   - `diag16/17/18/19`: подсветка надёжно зажигается **только при ALL‑LOW**; ни один
     одиночный пин (проверены все 30) её не включает (в `diag18` подсветка горела лишь
     на SYNC и FINAL = ALL‑LOW; 8 кандидатов дали 96 с темноты);
   - `diag21` (**leave‑one‑out**: держим ALL‑LOW и поднимаем по одному пину): подъём
     `PG9` и `PB4` **гасит** подсветку ⇒ они необходимы; в остальном — комбинация;
   - **Важная поправка по портам:** в FEX порты кодируются **A=1** (`leds_pin=(7,9)` =
     `pg09`; `gpio_pin_14=(2,10)` = `PB10`). Значит `lcd0_para` с `port=4` — это
     **PD** (шина LCD), а `gpio_para port=5` — это **PE**. Раньше путали PE/PD;
   - `diag24/v25`: `PD CFG before = 0x22222200 0x22222200 0x22222200 0x00002222` —
     **PD уже полностью в режиме LCD (mul=2)**: ядро пины LCD настраивает правильно
     (принудительный мукс ничего не меняет). PE оказался не при чём;
   - картинки НЕТ ни при `v23` (не трогаем PE/PD), ни при `v25` (форсим PD + подсветка);
   - `diag26` (дамп `LCDC0/TCON` @`0x01C0C000`): `[0x00]=0x80000000` (TCON enable),
     `[0x40]=0x800001e0`, `[0x44]=0xf0000009` (DCLK), тайминги **идеальны**:
     `[0x48]=0x031f01df` (800×480), `[0x4c]=0x041f00d7` (HT 1056 / HBP 216),
     `[0x50]=0x041a0022` (VT/VBP), `[0x54]=0x00090009` (spw 10); идут кадровые
     прерывания (≈60 Гц) ⇒ **контроллер реально выдаёт кадры**;
   - `diag27` (**raise‑sweep**: ALL‑LOW + по очереди поднимаем каждый пин): картинка
     не появилась; при подъёме пина подсветки она гасла (активный низ).
   - **ВЫВОД:** наше ядро **полностью инициализирует дисплей** (тайминги, PD‑мукс,
     TCON включён, DE/скалер работают), но **матрица ничего не показывает** ⇒ не хватает
     **board‑специфичной инициализации/питания панели**, которую делает вендорское ядро
     из board‑кода (в FEX её нет). Подсветка — активный низ, включается при ALL‑LOW.
8. **Отладка disp в ядре:** добавлены точечные `pr_info` (`[FBI] …`) в `dev_fb.c`
   (`Fb_Init`, вывод `b_init/disp_mode/output_type`), `dev_disp.c` (`DRV_lcd_open`),
   `disp_lcd.c` (`Disp_lcdc_init` + реальные параметры панели). `__inf` временами
   включался как `pr_info` (для диагностики), потом возвращён в `pr_debug` (иначе
   кадровый ISR затапливает лог).

### Проверка вендорского пути (стоковая система)

- Стоковое ядро возвращено на карту (`res/DATA01`=`res/ext/DATA01`,
  `stock_DATA01.uImage`, sha256 `d32f3b47…`), окружение — полный стоковый `res/`.
  На **вручную собранной** карте (свежий MBR + скопированный U‑Boot, FAT 1.8 ГБ)
  стоковая система **не поднялась** — чёрный экран без подсветки (наше ядро на той же
  карте грузилось, т.е. U‑Boot/DATA01 механизм рабочий).
- `20240628.img` — **частичный дамп (418 МБ)**, при этом в таблице разделов один
  раздел `start=2048, size=106493952` (≈50.8 ГБ). `res/DATA01` в нём **побайтово
  совпадает** с `stock_DATA01.uImage`.
- **Следующий шаг:** записать **полный `20240628.img`** родным загрузчиком/MBR
  (Win32 Disk Imager) и проверить, что **родная система грузится** (меню/дисплей).
  Если да — фундамент есть, дальше **портируем RetroArch на вендорский стек**; если
  нет — образ неполный, нужен полный сток.

Текущие образы: `out/diag3-sd.img` (2 ГБ; вендорский U‑Boot + наше ядро в `res/DATA01`,
полный стоковый `res/`). Последняя диагностическая сборка ядра — **v27**
(sha256 `e54509af…`). Помощники в temp/r3x: `build_diag_kernel*.sh`, `build_diag3.sh`,
`flash_kernel.sh` (запись ядра прямо на карту без перепрошивки), `flash_stock.sh`,
`read_diag*.sh`, `fex_*.py/.sh`, `find_bl.sh`, `dissect_libsys.sh`, `patch_targeted.py`.

**Ключевой факт (актуально):** наше ядро **корректно поднимает весь дисплейный тракт**
(PD в режиме LCD, TCON включён, кадры идут), но **матрица не показывает**. НОВОЕ
(см. §«Разбор стока» ниже): возможно, дело в самой панели — **в стоке нет панели
800×480**. Поэтому сначала подтверждаем вендорскую систему целиком (полный
`20240628.img`), параллельно переносим стоковую панель в наше ядро.

## Разбор стока: инициализация дисплея (полный отчёт — `../unpack/STOCK_DISPLAY_INIT.md`)

Восстановили символы из стоковых ядер (`vmlinux-to-elf`) и проследили весь путь
инициализации дисплея. Кратко:

- **`res/system.img`** — посторонний файл: x86-64 ядро **Ubuntu 16.04**, подписанное
  сертификатом **Canonical Secure Boot** (наш U-Boot лишь проверяет наличие файла).
- В `res` — **3 ядра и 3 FEX** для разных моделей. Панель **зашита в код** ядра:
  - **`res/DATA01`** (21.06) → **RGB 480×854** (dclk27, ht538/hbp48/hspw40, vt1740/vbp12/vspw3);
  - **`res/ext/DATA01`** (28.06) и **`libunity`** → **CPU-8080 320×240** (ILI9341/GC9306/GC9307,
    выбор из FEX `lcd0_para.lcd_type`; reset-GPIO `lcd_gpio_0`=PD2).
- **Панели 800×480 в стоке НЕТ** — наша `hv_800x480_td043` почти наверняка лишняя.
- CPU-панель требует **GPIO-reset + таблицу регистров** (`LCD_panel_init`/`ili9341_init`),
  которых в нашем ядре нет.
- Подсветка ядром не управляется (нет `lcd_bl_en`/`lcd_power`, `pwm_used=0`) — только
  юзерспейс (`/dev/driver_misc` ioctl `0x40046c04`).
- Следующий эксперимент: подменить в нашем ядре `LCD_cfg_panel_info` на стоковые
  тайминги **480×854** и проверить появление картинки; если нет — переносить
  CPU-инициализацию (адреса функций и логика — в отчёте).

## РЕШЕНИЕ дисплея: панель GC9307 (подтверждено на железе, 2026-10-08)

- Определили маркерами: устройство грузит **`res/ext/DATA01`** ⇒ панель —
  **GC9307, CPU‑8080, 320×240**, reset — `lcd_gpio_0`=**PD2**.
- Панель **зашита в ядро**, не в FEX. Перенесли в `drivers/video/sunxi/lcd/lcd0_panel_cfg.c`:
  - `LCD_cfg_panel_info`: `x=320 y=240 dclk=5 ht=329 hbp=5 hspw=3 vt=498 vbp=5 vspw=3
    lcd_if=1 lcd_cpu_if=4 lcd_frm=2 io_cfg0=0x10000000`;
  - `LCD_open_flow`/`LCD_close_flow` + `gc9307_init` (**85 команд**, извлечены из
    вендорского `res/ext/DATA01` автодизассемблером) + reset PD2 через `LCD_GPIO_write(sel,0,…)`.
- **Проверено на железе:** ядро **v30** вывело **цветные вертикальные полосы** → панель,
  TCON и CPU‑8080 работают.
- Подсветка — **активный низ** на пинах `gpio_para`. Ядро само включает её
  (`a13_backlight_on`: пины принудительно **выход+0**, иначе они по FEX — входы и ничего не выходит).
- Стоковые значения и трассировка — в `../unpack/STOCK_DISPLAY_INIT.md`.

## Интеграция Lakka (в работе, 2026-10-08)

- **Mainline u‑boot (`a13-olinuxino`) НЕ грузится** на этом устройстве (чёрный без подсветки).
  Рабочий путь — **вендорский u‑boot**.
- Схема карты: вендорский u‑boot + FAT c меткой `LAKKA`:
  `res/ext/DATA01` = наш Lakka‑uImage, `res/ext/DATA02` = CPU‑FEX (912 МГц),
  `res/system.img` (проверка наличия), `SYSTEM` (squashfs); плюс ext4 `LAKKA_DISK`.
  Ядро запускается встроенным initramfs LibreELEC; cmdline (FORCE) —
  `… boot=LABEL=LAKKA disk=LABEL=LAKKA_DISK`.
- **Проверено:** вендорский u‑boot поднимает наш Lakka‑uImage, **подсветка включается**
  (ядро стартует), но экран пустой → initramfs/`init` не доходил до userspace.
- Найдено и исправлено по ходу:
  - **`make image` пересоздаёт исходники ядра** и стирает правки (в т.ч. `lcd0_panel_cfg.c`!)
    → правки держать патчами в `projects/Allwinner/devices/A13_4C/patches/linux/`;
  - ядро 3.4 + gcc9: нужен `include/linux/compiler-gcc9.h` (иначе `BUILD_BUG` → asm `.err`)
    и `arch/arm/include/asm/compiler.h`: `__asmeq(x,y)` → `""` (иначе `put_user` → `.err`);
  - initramfs LibreELEC **без `/bin`** → шебанг `#!/bin/sh` не резолвится, init не стартует.
    **Добавлен симлинк `/bin → usr/bin`** (`/sbin → usr/sbin`);
  - `CONFIG_FRAMEBUFFER_CONSOLE` **выключен** → на экране нет boot‑текста;
  - `arch/arm/plat-sunxi/core.c`: LCD‑резерв `SZ_256M→SZ_64M` (RAM ровно 256 МиБ);
  - **zram** включён (`CONFIG_ZRAM=y` в `linux.arm.conf`);
  - FEX `target.boot_clock = **912`** МГц (запрос пользователя ~913; шаг A13 = 912).
- Артефакты: `out/lakka-a13.img` (mainline — не грузится), **`out/lakka-vendor-sd.img`**
  (вендор — рабочий), `out/lakka-uImage`, `out/lakka-zImage`.
- Осталось: добить initramfs (после `/bin`), затем **SYSTEM/RetroArch** — учесть риск:
  Lakka‑RetroArch рассчитан на **KMS/GL**, а ядро BSP — **fbdev+Mali** (нужен fbdev‑контекст).

## Прогресс: загрузка Lakka, обход systemd (обновление 2, 2026-10-08)

Довели **zImage + Lakka** до systemd, выявили и обошли ряд проблем:

**Что сделано/найдено:**
- **initramfs LibreELEC неполный**: добавили симлинки `/bin`,`/sbin`,`/lib`,`/lib64` → `usr/*`
  и **161 симлинк апплетов busybox** (`mount`, `awk`, `usleep`, …). Без них `init` не запускался.
- **Ядро не умело squashfs** → `CONFIG_SQUASHFS=y` (+zlib/lzo/xz). `SYSTEM` стоковой Lakka
  сжат **zstd** (ядро 3.4 не умеет) → пересобрали `SYSTEM` в **xz** (`mksquashfs`) и обновили `SYSTEM.md5`.
- cmdline переведён на **пути устройств**: `boot=/dev/mmcblk0p1 disk=/dev/mmcblk0p2`
  (busybox не разворачивает `LABEL=` без `/dev/disk/by-label`).
- **zram** (`CONFIG_ZRAM=y`); **FEX `target.boot_clock=912`**.
- **storage (`p2`)**: наш образ содержал ext4 с `metadata_csum` — **ядро 3.4 его не монтирует**
  (EINVAL). Обход: убрали `disk=` → `/storage` = tmpfs.
- **`CONFIG_OVERLAY_FS=y`** (нужен RetroArch для `/tmp/{cores,assets,overlays,…}`).
- Диагностика — через `A13TEST.TXT`/`KMSG.TXT`/`RETRO.LOG` на FAT и kmsg на экране.

**systemd 242 стартует, но падает:**
`systemd[1]: Failed to determine whether /sys is a mount point: Bad file descriptor`
→ `[!!!!!!] Failed to mount API filesystems` → `Freezing execution`.
- Тест‑бинарник (повторяет логику systemd) показал: **syscalls ядра исправны** —
  `open(O_PATH)`, `name_to_handle_at(fd,name[,AT_SYMLINK_FOLLOW])`,
  `name_to_handle_at(fd,"",AT_EMPTY_PATH)`, `fstatat` — все ок (для `/sys`,`/proc` `-EOPNOTSUPP`,
  для `/dev` 0; **EBADF нет**). Значит EBADF рождается внутри systemd (вероятно `chase_symlinks`), не в ядре.
- Обход «заставить `name_to_handle_at` возвращать `-EOPNOTSUPP`» — **не помог**.
- `mount --move` из LibreELEC‑init и `switch_root` на наш скрипт **проваливаются** →
  `Kernel panic: Attempted to kill init`.

**Решение — уйти от systemd:** задали **`rdinit=/a13init`** — собственный init, который сам
монтирует devtmpfs/proc/sys, `/flash` (FAT), `/newroot` (`SYSTEM` squashfs через loop),
tmpfs (`/run /var /tmp /storage`), overlay‑каталоги RetroArch, `devpts`/`dev/shm`,
затем `chroot /newroot` и запускает RetroArch (`/a13launch`). systemd и `mount --move` не используются.

**Артефакты:** `temp/r3x/a13init`, `a13launch` (вшиты в initramfs), `build_rdinit.sh`.
Юниты RetroArch в SYSTEM: `retroarch.target`/`retroarch.service` (`HOME=/storage`,
`ExecStart=/usr/bin/retroarch`), overlay‑маунты `tmp-*.mount`, `retroarch-config`, `libmali-setup`.

## ПРОРЫВ: RetroArch рисует и запускает игры (обновление 3, 2026-10-08)

Полный стек заработал: **своё ядро → свой init (`rdinit=/a13init`) → SYSTEM (squashfs xz) → RetroArch**.
Подтверждено на железе: RetroArch выводит картинку (сплошной цвет из тестового NES‑ROM) и **крутит
кадры** (анимация), ядро `fceumm` грузит NES‑ROM (`[libretro] Player 1..4 / Famicom`).

Ключевые решения:

1. **Видеодрайвер `sunxi`** (исходник `gfx/drivers/sunxi_gfx.c`): софт‑рендер через **pixman** +
   вывод через DISP‑слой. В стоковой сборке Lakka он **не был собран** (`HAVE_SUNXI=0`,
   `Makefile.common` не добавляет `-DHAVE_SUNXI`). Пересобрали вручную:
   `make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1 HAVE_FREETYPE=1`
   при `config.mk`: `HAVE_SUNXI=1` и `-DHAVE_SUNXI` в `CFLAGS`, удалив `obj-unix/release/gfx/video_driver.o`.
2. **Framebuffer должен быть 32bpp, ≥3 экрана** (драйвер пишет кадры ниже видимого экрана,
   `offset=(yres+i*src_height)*xres*4`). В `dev_fb.c` жёстко задали
   `init_para->format[0]=DISP_FORMAT_ARGB8888` (**32bpp**) и `init_para->buffer_num[0]=3`
   (было RGB565/16bpp и 2). Теперь `fb: 320x240 bpp=32 smem_len=0xe1000`.
3. **Запуск RetroArch от root**: SYSTEM‑`busybox` — setuid и понижает euid до 1000, из‑за чего
   `open("/dev/disp")` и `insmod` падали (`fd_disp=-1`, «Operation not permitted»). Решение — init
   (initramfs‑busybox, uid 0) сам делает `chroot /newroot /flash/retroarch …`, а привилегированные
   операции (mount debugfs, `insmod ump/mali/disp_ump`) выполняет **до chroot**.
4. **Логирование**: вывод RetroArch (`--verbose 2>&1`) → `RA.LOG` на FAT + фоновый `sync` каждые 2 с
   (иначе FAT не досинхронизируется при зависании). Диагностика процесса — `PSTAT.txt`
   (`/proc/<pid>/{status,wchan,maps}`, `task/*/wchan`).
5. **Дедлок меню**: при запуске **без контента** RetroArch виснет в `futex_wait` (главный поток);
   **с контентом (`-L core rom`) меню работает** и кадры идут. Временно: `menu_driver` и
   `audio_driver` = `null` не помогали; обход — всегда грузить контент.

Что ещё **не сделано**:
- **Ввод (кнопки)**: `CONFIG_INPUT_KEYBOARD` в ядре выключен → `/dev/input` пуст, udev в RetroArch
  не находит устройств. Вендор использует **кастомный keypad** (FEX `keypad_para` `kp_used=0`;
  есть `sun4i-keypad.c` = LRADC). Нужно включить/подключить драйвер кнопок и смапить их.
- **Звук**: `failed_to_start_audio_driver` (ALSA/`aplay` не поднялся).
- **Лончер**: свой (список ROM → запуск `retroarch -L <core> <rom>`), т.к. режим «меню без контента» виснет.

Артефакты: `temp/r3x/a13init` (init), `out/retroarch-sunxi` (пересобранный RetroArch),
`check/` — все скрипты, `check/retroarch-sunxi`, `check/a13init`, `check/vendinit.cpio`.

## ИСПРАВЛЕН дедлок меню RetroArch (обновление 4, 2026-10-08)

Меню RetroArch (rgui) без контента теперь **запускается и отображается**.

Причина дедлока (найдена через `gdbserver`/`gdb` в SYSTEM + `addr2line` по бинарю с debug_info):
- Главный поток висел в `sunxi_set_texture_enable()` → `sthread_join(_dispvars->vsync_thread)`
  (адрес `0x1b0ba8`, подтверждено дизасмом): при активации меню драйвер останавливал и
  **join'ил** фоновый vsync‑поток, который не завершался → `pthread_join` вечно ждал в futex.
- Второй поток (111) при этом спал в `threaded_worker()` (воркер очереди задач) — он невиновен.

Диагностика: `/flash/diag.sh` (хук из init) запускал `chroot /newroot /usr/bin/gdb /flash/retroarch -p PID`
и складывал `thread apply all bt full` в `/flash/BT.txt`. Адреса символизировались на хосте
`armv7a-…-addr2line/objdump`.

Фикс (`gfx/drivers/sunxi_gfx.c`):
- `sunxi_set_texture_enable()` больше **не** join'ит и не пересоздаёт vsync‑поток — только
  выставляет `menu_active = state`. Поток живёт весь срок жизни драйвера.
- `sunxi_gfx_free()` — всегда `keep_vsync=false; sthread_join(vsync_thread)`.
- `keep_vsync` сделан `volatile`.

Артефакты: `check/sunxi_gfx.c.menufix`, `check/build_ra_menufix.sh`, `check/retroarch-sunxi`
(пересобранный), а также `check/diag.sh`.

Что осталось: **ввод (кнопки)** (в ядре нет драйвера keypad → `/dev/input` пуст), **звук**, лончер.

## Сеть (VPN и мост в Windows)

- На Windows включён **AmneziaVPN (WireGuard)**; **WSL2 не видит VPN** (Windows 10,
  без mirrored-режима), и после включения VPN у WSL пропала сеть вообще.
- Поэтому все исходники качаются **через Windows**:
  - архивные — `curl.exe` из WSL (`seed_pkg.sh`), с fallback на
    `http://sources.libreelec.tv/mirror/<pkg>/<file>`;
  - git-пакеты — `seed_git.sh` качает **архив конкретного коммита** (`curl.exe`):
    GitHub `codeload…/tar.gz/<sha>`, GitLab `…/-/archive/<sha>/…tar.gz`; SHA
    резолвится через API; **сабмодули** тянутся рекурсивно через GitHub
    contents‑API. Скачанное кладётся в `sources/<pkg>/` и помечается
    `.libreelec-seeded` (в `scripts/get_git` добавлено принятие маркера), чтобы
    сборка не делала `git clone`.
- Авто-цикл `autoloop.sh`: собирает, при ошибке загрузки сам засеивает и
  перезапускает; применяет правки ядра. Логи: `~/r3x/build.log`, `~/r3x/autoloop.log`.

## Состояние авто‑цикла

**Сборка завершена успешно** (`[189/189] install image`, «Successful build»).
Авто‑цикл отработал 6 попыток, автоматически засеяв недостающие исходники
(архивные и git, включая сабмодули) через Windows‑мост, и остановился по успеху.

## Как продолжить / пересобрать

```bash
# в WSL, от пользователя agent:
bash /mnt/c/Users/INSOER~1/AppData/Local/Temp/opencode/r3x/run_loop.sh
# (соберёт, засеет недостающие исходники через Windows и доведёт образ)
```
Итоговый образ появляется в `~/r3x/lakka/target/`; его можно скопировать в
`lakka-a13/out/` (см. `copy_out.sh`).

## Важные оговорки по железу

- Стоковый **boot0/SPL + BSP u-boot** есть в `20240628.img` и используется в
  варианте A; исходников вендорного ядра нет — ядро собрано из community
  `linux-sunxi 3.4` с вендорским `.config` (совместимость LCD нужно проверить на железе).
- Для RetroArch на этом железе нужен **контекст Mali/fbdev** (KMS/DRM у BSP нет) —
  возможна доработка пакета RetroArch.
- Финальная проверка возможна только на реальном устройстве.

## Обновление 5 (2026-10-08, вечер) — кнопки, звук, авто-раздел и ГЛАВНОЕ: как делать загрузочный образ

### Работает
- **Кнопки**: причина «залипания» — наша `a13_backlight_on()` (в `lcd0_panel_cfg.c`) гнала
  `PE0..PE11` и `PB16` (кнопки!) как выходы LOW при инициализации LCD. Убрали эти строки.
  Раскладка Q2 снята эмпирически (dmesg/опрос): ↑=PE11, ↓=PE10, ←=PE9, →=PE8, A=PE2, B=PE4,
  X=PE3, Y=PE5, L=PE7, R=PE6, Start=PE0, Select=PB16, Menu=PE1 (вендорская `g_key_info_a13_x` НЕ подходит).
- **Звук**: `audio_driver="alsa"` (кодек sunxi).
- **Настройки сохраняются**: конфиг перенесён в `/storage/.config/retroarch/retroarch.cfg` (на FAT).
- **Меню быстрое**: убрано блокирующее `FBIO_WAITFORVSYNC` в пути меню (LCD vsync ~354 мс).
- **Ввод**: `input_joypad_driver="linuxraw"` + `CONFIG_INPUT_JOYDEV=y` (libudev в этом ядре не работает — нет `CONFIG_NET`).

### АВТО-РАСШИРЕНИЕ РАЗДЕЛА — ГЛАВНАЯ ОШИБКА И УРОК
- Причина «карта перестала грузиться, нет экрана/подсветки»: раннее авто-расширение **создало раздел в
  секторе 16** (offset 8 КБ) и отформатировало его — а там **`eGON.BT0`** (boot0). Затёрли загрузчик.
- Исправлено: авто-расширение создаёт `p3` **строго после p2** (`start=p2_start+p2_size+4096`,
  `end=total-1`) — область загрузчика не трогается. Плюс есть флаг `/flash/.expanding` (однократность).

### КАК ДЕЛАТЬ ЗАГРУЖАЮЩИЙСЯ ОБРАЗ (проверено)
- Рабочая карта = **`lakka-a13.img`** (собранный `make image`, `p1` с **offset 4 МиБ**) +
  **`boot_patch_4M.img`** (первые 4 МиБ рабочего образа: MBR + `eGON.BT0` + `U-Boot SPL 2019.04`).
- Загрузчик **`20240628.img` НЕ подходит** (его boot-область — другой u-boot, md5 `35668d…` vs рабочий `652423…`).
- Ядро вендорский u-boot ищет в **`res/ext/DATA01`** (uImage) и `res/ext/DATA02` (script.bin), НЕ в `res/DATA01`.
  (Дублировать в `res/DATA01`/`res/DATA02` — на случай boot.scr.)
- Ранний `build_sdimage.sh` ставил `p1` на сектор 2048 и брал загрузчик из `20240628.img` → **не грузился**.

### Репозиторий
- Всё для самостоятельной сборки выложено: https://github.com/insoerfly/q2retroarch

---

## Обновление 6 (2026-10-09) — zram, чистка логов, фикс конфига ядер, USB (в работе)

### Сделано
- **zram на ARM** (в этом дереве `zram`/`zsmalloc` помечены `depends on X86`): патчи
  `patches/0001..0003` (убрал X86, `set_pte`→`set_pte_ext`, `__flush_tlb_one`→`flush_tlb_kernel_page`).
  Включил `CONFIG_STAGING/ZRAM/ZSMALLOC`. `a13init` активирует swap **128 МБ**
  (на железе: `zram0 swap on (128M)`).
- **Убран отладочный спам**: `kernel/a13keys.c` (`PE=… (dbg N)` раз в секунду) и
  `retroarch/sunxi_gfx.c` (`SUNXIDBG`/`SUNXIFPS`, писались в лог каждый кадр, лог рос до мегабайт).
- **`a13init` генерирует ЧИСТЫЙ `retroarch.cfg`** (без копии `/etc/retroarch.cfg`).
  Раньше копия стока + дописывание давали **дубли ключей** (`libretro_directory="/tmp/cores"`
  и наш `/usr/lib/libretro`; `assets_directory=/tmp/*`), из-за чего RetroArch не находил ядра.
  Теперь:
  - `libretro_directory = "/usr/lib/libretro"`
  - `libretro_info_path = "/usr/lib/libretro"`
- **pcsx_rearmed — low-mem** (`TARGET_SIZE_2 = 23`, кэш dynarec 8 МБ вместо 16) в `SYSTEM`.
- **FEX Q2** (`bootloader/script_q2.bin`, 29096 б): `target.boot_clock = 912`,
  `usbc0.usb_port_type = 0` (DEVICE). Скрипт правки — `patches/fex-set-usb-device.py`.

### USB-гейджет (ACM + Mass Storage) — НЕ ДОВЕДЁН
- `USB0` переведён в **DEVICE** (и FEX, и конфиг ядра); UDC (`sw_usb_udc`) теперь
  регистрируется (патч `0004` — `g_udc_pdev` в device-only; FEX `usb_port_type=DEVICE`).
- `acm_ms` не привязывался из-за `max_speed=0` — патч `0005` (`.max_speed = USB_SPEED_HIGH`).
- Но bind всё равно падает: **`g_acm_ms gadget: unable to autoconfigure all endpoints`**
  (`-ENOTSUPP`). Причина: у sunxi `sw_udc` endpoints заданы жёстко (`bEndpointAddress`)
  и не проходят `usb_ep_autoconfig` — нужно доработать UDC. Итог: консоль/карта по USB
  пока **не поднимаются**; путь оставлен в ядре (модуль в initramfs), но не активен.

### Открытая проблема: игры не запускаются
- RetroArch **находит ядра** (фикс конфига выше) и **контент грузится**, но затем
  `[Core]: Content ran for a total of: 0 seconds` → `Unloading core` и система виснет.
  Воспроизводится на **fceumm+NES** и **snes9x2005+SNES** — то есть не ROM/ядро,
  а путь запуска контента (`sunxi_gfx.c`).
- Для отладки добавлен режим `MODE=test` в `a13init`: при файле `/flash/MODE=test`
  RetroArch стартует **напрямую** с ядром и ROM (мимо меню), пишет подробный `RA.LOG`.

### Артефакты
- Патчи ядра: `patches/` (см. `patches/README.md`).
- Обновлённые исходники: `kernel/a13keys.c`, `kernel/kernel.config`, `retroarch/sunxi_gfx.c`,
  `initramfs/a13init`, `bootloader/script_q2.bin`.
