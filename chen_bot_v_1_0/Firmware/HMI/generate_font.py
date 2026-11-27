"""Generate the small Noto Sans CJK LCD subset and its UTF-8 C strings."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
SIZE = 14


def c_string(value):
    encoded = "".join(
        chr(byte) if 32 <= byte < 127 and byte not in (34, 92) else f"\\{byte:03o}"
        for byte in value.encode("utf-8")
    )
    return '"' + encoded + '"'


def glyph(font, char):
    image = Image.new("L", (SIZE, SIZE))
    # Offset wide glyphs only as needed so the 14-pixel em stays in its cell.
    width = 8 if ord(char) < 128 else SIZE
    box = font.getbbox(char, anchor="ls")
    x = max(0, (width - (box[2] - box[0])) // 2) - box[0]
    ImageDraw.Draw(image).text((x, 11), char, fill=255, anchor="ls", font=font)
    rows = []
    for y in range(SIZE):
        rows.append(sum(1 << (SIZE-1-x) for x in range(SIZE) if image.getpixel((x, y)) >= 100))
    return rows


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("font", type=Path, help="NotoSansCJKsc-Regular.otf")
    options = args.parse_args()
    strings = json.loads((HERE/"text.json").read_text(encoding="utf-8"))
    # ASCII add digits, symbols and engineering units from numeric formatting.
    chars = sorted(set("".join(strings.values())) | {chr(i) for i in range(32, 127)})
    font = ImageFont.truetype(str(options.font), SIZE)
    rows = []
    for char in chars:
        bits = ", ".join(f"0x{x:04x}" for x in glyph(font, char))
        rows.append(f"    {{0x{ord(char):04x}, {{{bits}}}}},")
    with (HERE/"hmi_font.h").open("w", encoding="ascii", newline="\n") as out:
        print("/* Generated from Noto Sans CJK SC. See FONT_LICENSE.txt. */", file=out)
        print("#define GLYPH_SIZE 14u\nstatic const Glyph font[] = {", file=out)
        print(*rows, sep="\n", file=out)
        print("};", file=out)
    with (HERE/"hmi_text.h").open("w", encoding="ascii", newline="\n") as out:
        print("/* Generated from text.json. */", file=out)
        for key, value in strings.items():
            print(f"#define T_{key} {c_string(value)}", file=out)


if __name__ == "__main__":
    main()
