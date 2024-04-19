import os
import sys
from PIL import Image


def main(complete_filename: str):
    name = os.path.splitext(complete_filename)[0]
    img = Image.open(complete_filename).convert("RGB")
    with open(name + '.h', 'w') as target_file:
        target_file.write("static const uint32_t {0}_W = {1};\n".format(name, img.width))
        target_file.write("static const uint32_t {0}_H = {1};\n".format(name, img.height))
        target_file.write("static const uint8_t {0}[] = {{\n".format(name))
        for y in range(img.height):
            for x in range(img.width):
                r, g, b = img.getpixel((x, y))
                target_file.write("{0},{1},{2},".format(r, g, b))
            target_file.write("\n")

        target_file.write("};")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("No file to convert")
    else:
        main(sys.argv[1])
