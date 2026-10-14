# J2ME (телефонные Java‑игры) на Q2/A13 — как вернуть

Статус: **убрано** (2026‑10‑14). Ниже — всё, что нужно, чтобы быстро вернуть и дособрать.

## Суть задачи
Запускать J2ME‑игры (`.jar` от старых телефонов). Наш SoC — **A13 / ARMv7 (32‑бит)**.

## Что работает как база (ARM32!)
Пакет **`freej2me-anbernic`** (репозиторий `MizumiKawai/freej2me-anbernic`) — готовый набор под armhf:
- `jdk/` — **ARM32 JRE (Zulu 8/JDK8)**, `jdk/bin/java` (ELF 32‑bit ARM armhf) + `jdk/lib/*.so`;
- `jdk/bin/freej2me-sdl.jar` — FreeJ2ME (SDL‑сборка);
- `sdl_interface` — SDL2‑мост (видео из stdout java → рендер SDL2; ввод джойпада по IPC);
- `edge.jar` — тест‑игра; `keymap.cfg` — раскладка.

Запуск (из их `j2metest.sh`, но правильнее — через пайп):
```sh
JAVA_HOME=./jdk ; PATH=$JAVA_HOME/bin:$PATH
./jdk/bin/java -jar ./jdk/bin/freej2me-sdl.jar GAME.jar 240 320 30 2>/dev/null | ./sdl_interface 240 320
```
`java` кладёт кадры в stdout, `sdl_interface` читает и рисует (SDL2), ввод шлёт в java (через stderr/IPC).

## ГЛАВНЫЙ БЛОКЕР — SDL2 video backend
- Наш драйвер видео — **BSP sunxi fb** (`CONFIG_FB_SUNXI`, `/dev/fb0`), **нет mainline DRM/KMS** →
  `kmsdrm`‑бэкенд SDL2 не подойдёт.
- Значит нужен **`fbcon`‑бэкенд SDL2**. В ядре он есть:
  `CONFIG_FRAMEBUFFER_CONSOLE=y`, `CONFIG_FB=y`, `CONFIG_VT=y` — ок.
- НО **Debian‑SDL2 (2.0.14) собран без `fbcon`** (в строках только `x11/wayland/dummy`) →
  с ним `sdl_interface` не покажет картинку.

**Вывод:** надо собрать **libSDL2‑2.0.so.0 из исходников**
`--enable-video-fbcon --disable-video-x11 --disable-video-wayland ...`
под наш toolchain (`armv7a-libreelec-linux-gnueabi-`), и положить рядом с `sdl_interface`.

## Другие пути (архив)
- `reesque/J2ME-Pak` — готовый FreeJ2ME, но **ARM64 (aarch64) only** → не подходит.
- `aweigit/freej2me-miyoomini`, `libretro/libretro-JamVM`, `hex007/freej2me` (исходники FreeJ2ME) — на будущее.
- Ядро `freej2me_libretro.so` (для RA) — есть только под aarch64; под armv7 надо собирать из
  `TASEmulators/freej2me-plus`, плюс всё равно нужен JVM.
- Zulu **ARM32 JRE8**: `https://cdn.azul.com/zulu-embedded/bin/zulu8.76.0.17-ca-jdk8.0.402-linux_aarch32hf.tar.gz`.

## Где что было на карте
- `E:\j2me\` — пакет anbernic (jdk + jar + sdl_interface + `lib/libSDL2-2.0.so.0`).
- `E:\j2me\run.sh` — обёртка запуска (в репо: `j2me/run.sh`).
- `a13init` был режим `MODE=j2me` (запускал `/flash/j2me/run.sh`); **откачен**.

## Как вернуть (шаги)
1. Скачать `MizumiKawai/freej2me-anbernic` (zip) → распаковать в `/flash/j2me`.
2. Собрать **SDL2 с fbcon** под ARMv7 → положить `libSDL2-2.0.so.0` в `/flash/j2me/lib`.
3. `j2me/run.sh`: `JAVA_HOME=/flash/j2me/jdk`, `LD_LIBRARY_PATH=/flash/j2me/lib:$JAVA_HOME/lib`,
   `SDL_VIDEODRIVER=fbcon`, `SDL_JOYSTICK_DEVICE=/dev/input/js0`,
   `java -jar freej2me-sdl.jar GAME.jar 240 320 30 | sdl_interface 240 320`.
4. Добавить запуск: либо `MODE=j2me` в `a13init`, либо пункт меню (в RA — не зашло).
5. Игры — в `/storage/roms/j2me/*.jar`.
