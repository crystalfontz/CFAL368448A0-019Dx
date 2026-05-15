import sys, struct
from PIL import Image
TARGET_W, TARGET_H = 368, 448
OUT_FILE = sys.argv[2] if len(sys.argv) > 2 else "IMAGE.BMP"
img = Image.open(sys.argv[1]).convert("RGB")
img.thumbnail((TARGET_W, TARGET_H), Image.LANCZOS)
canvas = Image.new("RGB", (TARGET_W, TARGET_H), (0, 0, 0))
ox = (TARGET_W - img.width)  // 2
oy = (TARGET_H - img.height) // 2
canvas.paste(img, (ox, oy))
pixels = canvas.load()
stride = (TARGET_W * 2 + 3) & ~3       # row stride padded to 4 bytes
data_offset = 14 + 40 + 12             # file + info + bitfield headers
file_size = data_offset + stride * TARGET_H
with open(OUT_FILE, "wb") as f:
  # BITMAPFILEHEADER (14 bytes)
  f.write(b"BM")
  f.write(struct.pack("<I", file_size))
  f.write(struct.pack("<HH", 0, 0))
  f.write(struct.pack("<I", data_offset))
  # BITMAPINFOHEADER (40 bytes)
  f.write(struct.pack("<I", 40))
  f.write(struct.pack("<i", TARGET_W))
  f.write(struct.pack("<i", -TARGET_H)) # negative = top-down row order
  f.write(struct.pack("<H", 1))
  f.write(struct.pack("<H", 16))        # 16 bits per pixel
  f.write(struct.pack("<I", 3))         # BI_BITFIELDS compression
  f.write(struct.pack("<I", stride * TARGET_H))
  f.write(struct.pack("<ii", 2835, 2835))
  f.write(struct.pack("<II", 0, 0))
  # RGB565 channel masks (12 bytes)
  f.write(struct.pack("<I", 0xF800))    # red   mask
  f.write(struct.pack("<I", 0x07E0))    # green mask
  f.write(struct.pack("<I", 0x001F))    # blue  mask
  # Pixel data (top-down, little-endian RGB565)
  for y in range(TARGET_H):
    row = b""
    for x in range(TARGET_W):
      r, g, b = pixels[x, y]
      row += struct.pack("<H", ((r>>3)<<11)|((g>>2)<<5)|(b>>3))
    while len(row) % 4: row += b"\x00"
    f.write(row)
print(f"Saved {OUT_FILE}")