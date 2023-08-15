import sys
import math

def write_mif(out, data : bytes, width: int, depth: int, byteorder : str = 'little'):
    chunk_size = (width + 7) // 8
    out.write(f"DEPTH = {depth};\nWIDTH = {width};\nDATA_RADIX = BIN;\nADDRESS_RADIX = HEX;\nCONTENT\nBEGIN\n")

    addr_len = math.ceil(math.log(depth, 16))
    fmt_data = f'0{width}b'
    fmt_addr = f'0{addr_len}X'
    addr = 0
    for ofs in range(0, len(data), chunk_size):
        v = int.from_bytes(data[ofs:ofs+chunk_size], byteorder=byteorder)
        data_str = format(v, fmt_data)
        addr_str = format(addr, fmt_addr)
        out.write(f'{addr_str} : {data_str};\n')
        addr += 1
    
    zero = format(0, fmt_data)
    while addr < depth:
        addr_str = format(addr, fmt_addr)
        out.write(f'{addr_str} : {zero};\n')
        addr += 1

    out.write("END;\n")



if __name__ == '__main__':
    if len(sys.argv) >= 5:
        in_name = sys.argv[1]
        out_name = sys.argv[2]
        width = int(sys.argv[3])
        depth = int(sys.argv[4])
        if len(sys.argv) == 6:
            byteorder = sys.argv[5]
        else:
            byteorder = 'little'
    else:
        print(f"Usage: {sys.argv[0]} <INPUT> <OUTPUT.MIF> <WIDTH> <DEPTH> [ENDIAN]")
        sys.exit(-1)

    data = open(in_name, 'rb').read()
    with open(out_name, 'wt') as fp:
        write_mif(fp, data, width, depth, byteorder)

