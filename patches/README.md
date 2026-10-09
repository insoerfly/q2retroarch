# patches/ — правки к ядру/RetroArch/прошивке для A13 (Q2)

Патчи применяются поверх `linux-sunxi 3.4` (`d47d367036be38c5180632ec8a3ad169a4593a88`)
и `retroarch-ad89b0c` (v1.14.0). Пути — относительно корня соответствующего дерева.

| # | Файл | Что делает |
|---|------|-----------|
| 0001 | `0001-zram-Kconfig-no-x86.patch` | `zram` в этом дереве помечен `depends on BLOCK && SYSFS && X86`; убираем `X86`, чтобы собрать на ARM. |
| 0002 | `0002-zsmalloc-Kconfig-no-x86.patch` | `zsmalloc` — `depends on X86` → `depends on BLOCK`. |
| 0003 | `0003-zsmalloc-arm-port.patch` | Порт `zsmalloc` на ARM: `set_pte`→`set_pte_ext`, `__flush_tlb_one`→`flush_tlb_kernel_page`. |
| 0004 | `0004-sw_udc-g_udc_pdev.patch` | `sw_udc.c`: объявить `g_udc_pdev` вне `#ifdef ..._USB0_OTG` и заполнять в device-only пробе — иначе не компилируется в `DEVICE_ONLY`. |
| 0005 | `0005-acm_ms-max_speed.patch` | `acm_ms.c`: выставить `.max_speed = USB_SPEED_HIGH`, иначе `sw_udc` отвергает драйвер (`speed 0`). |
| 0006 | `0006-sw_udc-int-ep-first.patch` | `sw_udc.c`: `ep5-int` (interrupt) поставить первым в `ep_list`, иначе `usb_ep_autoconfig` отдаёт ACM-notify bulk-эндпоинт и композит не собирается. |
| —    | `kernel/acm_ms.c` | Полная замена `drivers/usb/gadget/acm_ms.c`: добавлен параметр `use_ms`, масса-сторадж **выключен по умолчанию** (`static int use_ms = 0`) — поднимается только CDC-ACM (иначе хост каждые ~20 с переинициализирует композит из-за LUN «no medium»). |
| —    | `fex-set-usb-device.py` | FEX: `usbc0.usb_port_type` 1(HOST)→0(DEVICE), иначе менеджер sunxi не создаёт UDC. |

Применение:

```sh
cd <linux-src>
for p in 0001 0002 0003 0004; do patch -p0 < .../patches/$p-*.patch; done
# 0005 — вставить строку .max_speed в drivers/usb/gadget/acm_ms.c (см. файл)
python3 .../patches/fex-set-usb-device.py bootloader/script_q2.bin
```

## Статус (важно)

- **zram** (0001–0003) — рабочий, включён в ядро, активируется в `a13init` (swap 128 МБ).
- **USB-гейджет** (0004–0006, `acm_ms.c`, FEX) — **работает**: `sw_udc` регистрируется,
  композит поднимается в режиме **только CDC-ACM** (масса-сторадж выключен), в Windows
  появляется COM-порт и доступен root-shell (115200 8N1, `chroot /newroot /bin/sh -i`
  на `ttyGS0`). Масса-сторадж отключён намеренно: LUN без носителя заставлял хост каждые
  ~20 с переинициализировать композит, и порт не открывался.

## Другое (не в патчах)

- `retroarch/sunxi_gfx.c` — убраны отладочные `SUNXIDBG`/`SUNXIFPS` принты
  (спамили в лог каждый кадр).
- `kernel/a13keys.c` — убран дебаг-спам.
- `initramfs/a13init` — генерирует чистый `retroarch.cfg` (без дублей ключей),
  `libretro_directory=/usr/lib/libretro`, `libretro_info_path=/usr/lib/libretro`,
  активирует zram, тянет USB-гейджет и режим `MODE=test`.
- `bootloader/script_q2.bin` — FEX Q2 (29096 б, 912 МГц, `usb_port_type=0`).
