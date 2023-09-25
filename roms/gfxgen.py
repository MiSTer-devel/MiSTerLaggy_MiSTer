import sys
from PIL import Image

sheet = {}

def import_font():
    mapping = {}
    for x in range(64):
        mapping[x] = x + 0x20
    for x in range(32):
        mapping[x+64] = x
    for x in range(32):
        mapping[x+96] = x + 0x60

    im = Image.open('atarifont.png')
    data = im.getdata()
    data_1bit = [ 0 if x == (0,0,0) else 1 for x in data ]
    idx = 0
    for tile_start in range(0, len(data_1bit), 64):
        sheet[mapping[idx]] = data_1bit[tile_start:tile_start+64]
        idx += 1

def write_sheet(out):
    blank = [0] * (8*8)
    out.write(f"DEPTH = 2048;\nWIDTH = 32;\nDATA_RADIX = BIN;\nADDRESS_RADIX = HEX;\nCONTENT\nBEGIN\n")

    fmt_data = '032b'
    fmt_addr = '04X'
    addr = 0

    for idx in range(256):
        byte_data = sheet.get(idx, blank)
        for row_start in range(0, 64, 8):
            row32 = 0
            for b in byte_data[row_start:row_start+8]:
                row32 = (row32 << 4) | (b & 0x0f)
            data_str = format(row32, fmt_data)
            addr_str = format(addr, fmt_addr)
            out.write(f'{addr_str} : {data_str};\n')
            addr += 1
    out.write("END;\n")


import_font()
write_sheet(open('gfx.mif', 'wt'))