import shutil
import sys

from elftools.elf.elffile import ELFFile

shutil.copyfile(sys.argv[1], sys.argv[2])
with open(sys.argv[2], "r+b") as stream:
    elf = ELFFile(stream)
    header = elf.header
    header["e_flags"] = int(sys.argv[3], 0)
    encoded_header = elf.structs.Elf_Ehdr.build(header)
    stream.seek(0)
    stream.write(encoded_header)
