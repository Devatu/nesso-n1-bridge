from PIL import Image

src = r"C:\Users\Devin\Nesso N1\firmware\assets\qvantum-logo.png"
out_h = r"C:\Users\Devin\Nesso N1\firmware\nesso_n1_bridge\qvantum_logo.h"
SIZE = 32

img = Image.open(src).convert("RGBA")
w, h = img.size
side = max(w, h)
canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
canvas.paste(img, ((side - w) // 2, (side - h) // 2), img)
img = canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def rgba_to_rgb565(r, g, b, a):
    if a < 40:
        return 0x0000
    if r > 230 and g > 230 and b > 230:
        return 0x0000  # white bg -> screen black
    r = (r * a) // 255
    g = (g * a) // 255
    b = (b * a) // 255
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


pixels = []
for y in range(SIZE):
    for x in range(SIZE):
        pixels.append(rgba_to_rgb565(*img.getpixel((x, y))))

with open(out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef QVANTUM_LOGO_H\n#define QVANTUM_LOGO_H\n\n")
    f.write("#include <stdint.h>\n#include <pgmspace.h>\n\n")
    f.write(f"#define QVANTUM_LOGO_W {SIZE}\n")
    f.write(f"#define QVANTUM_LOGO_H {SIZE}\n\n")
    f.write("static const uint16_t QVANTUM_LOGO_RGB565[] PROGMEM = {\n")
    for i, v in enumerate(pixels):
        if i % 12 == 0:
            f.write("  ")
        f.write(f"0x{v:04X},")
        f.write("\n" if i % 12 == 11 else " ")
    if len(pixels) % 12:
        f.write("\n")
    f.write("};\n\n#endif\n")

print("wrote", out_h, "nonblack", sum(1 for p in pixels if p))
