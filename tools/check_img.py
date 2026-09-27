from PIL import Image

im = Image.open("screenshot_now.png")
print("Format:", im.format, "Size:", im.size, "Mode:", im.mode)
colors = im.getcolors(maxcolors=256)
print("Colors in screenshot:", colors)
