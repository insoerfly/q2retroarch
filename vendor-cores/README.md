# vendor-cores/ — ядра эмуляторов из вендорской прошивки (`20240628.img` → раздел `res`, ext3)

Извлечены из `unpack/rootfs_arm/lib/`.

| Файл | Что | libretro? |
|------|-----|-----------|
| `libemulpsx.so` | Sony PlayStation (PCSX-ReARMed v1.9, `new_dyna`, `GPUrearmedCallbacks`) | **ДА** (экспортирует `retro_*`) |
| `libemulnes.so` | NES | нет (кастомный API вендора) |
| `libfceumm.so` | NES (FCEUmm) | нет |
| `libemulsfc.so` | SNES | нет |
| `libemulmd.so` | Sega Mega Drive | нет |
| `libemulgba.so` | GBA | нет |
| `libemula26.so` | Atari 2600 | нет |
| `libemula78.so` | Atari 7800 | нет |
| `libemulfbalpha.so` | FinalBurn Alpha | нет |
| `libemulmame2003p.so` | MAME 2003 Plus | нет |

Только `libemulpsx.so` — настоящий libretro-ядро. Остальные собственного API вендора и
работают только с их фронтендом (MiniGUI+SDL), не с RetroArch.

## Как подключён вендорский PSX

В `SYSTEM/usr/lib/libretro/` добавлены:
- `vendpsx_libretro.so` — копия `libemulpsx.so`;
- `vendpsx_libretro.info` — с `display_name = "vend Sony - PlayStation (pcsx_rearmed)"`
  (префикс `vend`, чтобы отличать от нашего `pcsx_rearmed`).

Требуется BIOS: `system_directory = "/storage/bios"`, файл `scph*.bin`
(ядро ищет `scphXXXX.bin`).
