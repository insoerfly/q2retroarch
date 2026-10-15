#!/usr/bin/env python3
"""cover_tool.py - small GUI to batch-convert box-art images into launcher covers.

Converts PNG/JPG/BMP/GIF/WebP/... from a source folder into the device format:
  <name>.565 = "C565" + u16 width + u16 height + width*height little-endian RGB565
so the MP5 X7 launcher can show a cover for each game (cover name == game name).

Run on Windows:   py cover_tool.py
Requires Pillow:  py -m pip install pillow
"""
import os
import threading
import queue
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

try:
    from PIL import Image
    _PIL_OK = True
except Exception as _e:            # pragma: no cover
    Image = None
    _PIL_OK = False
    _PIL_ERR = str(_e)

EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tif", ".tiff")


def write565(im, path):
    w, h = im.size
    px = im.load()
    buf = bytearray(b"C565" + bytes([w & 0xFF, w >> 8, h & 0xFF, h >> 8]))
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            buf += bytes([v & 0xFF, v >> 8])
    with open(path, "wb") as f:
        f.write(buf)


class App:
    def __init__(self, root):
        self.root = root
        self.q = queue.Queue()
        self.stop = False
        self.worker = None

        root.title("Cover converter (.565) - MP5 X7 launcher")
        root.minsize(620, 460)

        frm = ttk.Frame(root, padding=10)
        frm.pack(fill="both", expand=True)

        self.src = tk.StringVar()
        self.dst = tk.StringVar()
        self.maxw = tk.StringVar(value="160")
        self.maxh = tk.StringVar(value="224")
        self.png = tk.BooleanVar(value=True)
        self.rec = tk.BooleanVar(value=True)

        self._row(frm, 0, "Source folder:", self.src, self._pick_src)
        self._row(frm, 1, "Output folder:", self.dst, self._pick_dst)
        ttk.Button(frm, text="Output = Source", command=lambda: self.dst.set(self.src.get())).grid(
            row=2, column=1, sticky="w", pady=2)

        size = ttk.Frame(frm)
        size.grid(row=3, column=0, columnspan=3, sticky="w", pady=(6, 0))
        ttk.Label(size, text="Max size (W x H):").pack(side="left")
        ttk.Entry(size, textvariable=self.maxw, width=6).pack(side="left", padx=(6, 2))
        ttk.Label(size, text="x").pack(side="left")
        ttk.Entry(size, textvariable=self.maxh, width=6).pack(side="left", padx=(2, 12))
        ttk.Checkbutton(size, text="also save .png thumbs", variable=self.png).pack(side="left")
        ttk.Checkbutton(size, text="recursive", variable=self.rec).pack(side="left", padx=(10, 0))

        btns = ttk.Frame(frm)
        btns.grid(row=4, column=0, columnspan=3, sticky="ew", pady=(10, 4))
        self.go = ttk.Button(btns, text="Convert", command=self._start)
        self.go.pack(side="left")
        self.stopbtn = ttk.Button(btns, text="Stop", command=self._stop, state="disabled")
        self.stopbtn.pack(side="left", padx=6)

        self.pb = ttk.Progressbar(frm, mode="determinate")
        self.pb.grid(row=5, column=0, columnspan=3, sticky="ew", pady=(2, 6))

        self.log = tk.Text(frm, height=14, wrap="none")
        self.log.grid(row=6, column=0, columnspan=3, sticky="nsew")
        sb = ttk.Scrollbar(frm, command=self.log.yview)
        sb.grid(row=6, column=3, sticky="ns")
        self.log.configure(yscrollcommand=sb.set)

        frm.columnconfigure(1, weight=1)
        frm.rowconfigure(6, weight=1)

        if not _PIL_OK:
            self._log("Pillow not found: %s" % _PIL_ERR)
            self._log("Install it:  py -m pip install pillow")
            self.go.configure(state="disabled")

        root.after(100, self._drain)

    def _row(self, parent, r, label, var, cb):
        ttk.Label(parent, text=label).grid(row=r, column=0, sticky="w", pady=2)
        ttk.Entry(parent, textvariable=var).grid(row=r, column=1, sticky="ew", pady=2)
        ttk.Button(parent, text="Browse...", command=cb).grid(row=r, column=2, sticky="w", padx=(6, 0))

    def _pick_src(self):
        d = filedialog.askdirectory(title="Source folder")
        if d:
            self.src.set(d)

    def _pick_dst(self):
        d = filedialog.askdirectory(title="Output folder")
        if d:
            self.dst.set(d)

    def _log(self, s):
        self.q.put(("log", s))

    def _drain(self):
        try:
            while True:
                kind, val = self.q.get_nowait()
                if kind == "log":
                    self.log.insert("end", val + "\n")
                    self.log.see("end")
                elif kind == "prog":
                    done, total = val
                    self.pb["maximum"] = total
                    self.pb["value"] = done
                elif kind == "done":
                    self.go.configure(state="normal")
                    self.stopbtn.configure(state="disabled")
                    self.worker = None
        except queue.Empty:
            pass
        self.root.after(100, self._drain)

    def _start(self):
        if self.worker is not None:
            return
        src = self.src.get().strip()
        dst = self.dst.get().strip() or src
        if not os.path.isdir(src):
            messagebox.showerror("Cover converter", "Source folder is not valid.")
            return
        try:
            mw = int(self.maxw.get())
            mh = int(self.maxh.get())
            if mw < 1 or mh < 1:
                raise ValueError
        except ValueError:
            messagebox.showerror("Cover converter", "Max size must be positive integers.")
            return
        os.makedirs(dst, exist_ok=True)
        self.stop = False
        self.go.configure(state="disabled")
        self.stopbtn.configure(state="normal")
        self.worker = threading.Thread(
            target=self._run, args=(src, dst, mw, mh, self.png.get(), self.rec.get()), daemon=True)
        self.worker.start()

    def _stop(self):
        self.stop = True

    def _collect(self, src, recursive):
        out = []
        if recursive:
            for base, _dirs, files in os.walk(src):
                for f in files:
                    if f.lower().endswith(EXTS):
                        out.append(os.path.join(base, f))
        else:
            for f in os.listdir(src):
                p = os.path.join(src, f)
                if os.path.isfile(p) and f.lower().endswith(EXTS):
                    out.append(p)
        return sorted(out)

    def _run(self, src, dst, mw, mh, also_png, recursive):
        files = self._collect(src, recursive)
        self._log("Found %d image(s). Output -> %s" % (len(files), dst))
        done = 0
        for p in files:
            if self.stop:
                self._log("Stopped.")
                break
            name = os.path.splitext(os.path.basename(p))[0]
            try:
                im = Image.open(p).convert("RGB")
                im.thumbnail((mw, mh), Image.LANCZOS)
                write565(im, os.path.join(dst, name + ".565"))
                if also_png:
                    im.save(os.path.join(dst, name + ".png"))
                done += 1
                self._log("ok   %s  (%dx%d)" % (os.path.basename(p), im.size[0], im.size[1]))
            except Exception as e:
                self._log("fail %s  (%s)" % (os.path.basename(p), e))
            self.q.put(("prog", (done, len(files))))
        self._log("Done: %d converted." % done)
        self.q.put(("done", None))


def main():
    root = tk.Tk()
    try:
        style = ttk.Style()
        style.theme_use("vista" if "vista" in style.theme_names() else style.theme_use())
    except Exception:
        pass
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
