#!/usr/bin/env python3
"""Generate the serial-debug app icon (PNG) with PIL."""
from PIL import Image, ImageDraw

SIZE = 256
img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# Background rounded rect (radius 48), vertical gradient #24283b -> #16161e
def grad_color(y):
    t = y / SIZE
    r = int(0x24 + (0x16 - 0x24) * t)
    g = int(0x28 + (0x16 - 0x28) * t)
    b = int(0x3b + (0x1e - 0x3b) * t)
    return (r, g, b, 255)

# Draw rounded rect rows for a smooth gradient
mask = Image.new("L", (SIZE, SIZE), 0)
md = ImageDraw.Draw(mask)
md.rounded_rectangle([2, 2, SIZE-3, SIZE-3], radius=48, fill=255)
for y in range(2, SIZE-2):
    d.line([(2, y), (SIZE-3, y)], fill=grad_color(y))
img.putalpha(mask)

# Color palette
BLUE = (122, 162, 247, 255)
GREEN = (158, 206, 106, 255)
W = 10  # stroke width

def draw_rect(cx, cy, w, h):
    d.rounded_rectangle([cx-w/2, cy-h/2, cx+w/2, cy+h/2], radius=8, outline=BLUE, width=W)

cx, cy = SIZE/2, SIZE/2
# Outer rect 152x92
draw_rect(cx, cy, 152, 92)
# Inner rect 104x56
draw_rect(cx, cy, 104, 56)
# Horizontal midline
d.line([(cx-52+2, cy), (cx+52-2, cy)], fill=BLUE, width=W)
# Vertical midline
d.line([(cx, cy-28+2), (cx, cy+28-2)], fill=BLUE, width=W)
# Pins (small horizontal ticks)
ticks = [-14, 14]
for ty in ticks:
    d.line([(cx-52, cy+ty), (cx-14, cy+ty)], fill=BLUE, width=W)
    d.line([(cx+14, cy+ty), (cx+52, cy+ty)], fill=BLUE, width=W)
# Center LED dot
d.ellipse([cx-9, cy-9, cx+9, cy+9], fill=GREEN)

img.save("serial-debug.png")
print("saved serial-debug.png", img.size)