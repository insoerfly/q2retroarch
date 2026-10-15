# cover_tool — box-art covers for the MP5 X7 launcher

Batch-converts images (PNG/JPG/BMP/GIF/WebP/…) into the launcher's cover format:

```
.565 file = "C565" + u16 width + u16 height + width*height little-endian RGB565
```

Cover file name == game name (the ROM file name without extension). On the device the
launcher looks for `<name>.565` next to the ROM and in `covers/` folders, e.g.:

```
/mnt/diska/apps/game_launcher/<Name>.565            (next to the ROM)
/mnt/diska/apps/game_launcher/covers/<Name>.565
/mnt/diska/apps/covers/<Name>.565
/mnt/diska/covers/<Name>.565
```

## cover_tool.html  (recommended - no install)
Open in Chrome/Edge. Drag images in (or pick files), choose the output folder
(File System Access API writes directly to it; without it, files download), set the
max size, and hit **Convert**. PNG thumbnails are optional.

## cover_tool.py  (Python + Pillow)
Native window version with the same options (source/output folders, max W×H,
save .png thumbs, recursive scan).

```
py -m pip install pillow
py cover_tool.py
```

If `py`/`python` is broken on your machine, use the HTML version or run the Python one
from WSL (`python3 cover_tool.py`, needs `python3-tk` and a display).
