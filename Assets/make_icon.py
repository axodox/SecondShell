#Draws the SecondShell mark (UIDesign.md) at each icon size, supersampled: python make_icon.py SecondShell.ico <preview prefix>.
#has an outline of the opposite lightness, so the mark reads on light and dark backgrounds alike.
import sys
from PIL import Image, ImageDraw

BACK = (59, 53, 80, 255)         #dark slate violet
BACK_EDGE = (190, 180, 240, 255) #light: separates it from dark backgrounds
FRONT = (139, 124, 255, 255)     #the agent colour
FRONT_EDGE = (27, 27, 31, 255)   #dark: separates it from light backgrounds
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]

def mark(size):
    scale = 8
    canvas = size * scale
    unit = canvas / 18
    edge = round(max(1, size / 48) * scale)  #a pixel at small sizes, thin at large ones
    radius = 3.2 * unit
    image = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0.8 * unit, 0.8 * unit, 12.6 * unit, 12.6 * unit], radius, fill=BACK, outline=BACK_EDGE, width=edge)
    draw.rounded_rectangle([5.4 * unit, 5.4 * unit, 17.2 * unit, 17.2 * unit], radius, fill=FRONT, outline=FRONT_EDGE, width=edge)
    return image.resize((size, size), Image.LANCZOS)

images = [mark(size) for size in SIZES]
images[-1].save(sys.argv[1], format="ICO", sizes=[(s, s) for s in SIZES], append_images=images[:-1])

#Previews on a light and a dark background, at 32 px enlarged and at 256 px
for name, background in (("light", (243, 243, 243, 255)), ("dark", (32, 32, 36, 255))):
    for size in (32, 256):
        preview = Image.new("RGBA", (size, size), background)
        preview.alpha_composite(mark(size))
        preview.resize((256, 256), Image.NEAREST if size < 256 else Image.LANCZOS).save(sys.argv[2] + f"-{name}-{size}.png")
