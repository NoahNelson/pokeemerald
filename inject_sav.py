'''
This script copies a .sav file into the region of the ROM where the
save data is stored.
'''

import sys

SAVE_SECTOR_SIZE = 0x1000

def copy_sector(in_f, out_f, in_off, out_off):
    in_f.seek(in_off)
    out_f.seek(out_off)
    sector = in_f.read(SAVE_SECTOR_SIZE)
    out_f.write(sector)

if __name__ == '__main__':
    in_fname = sys.argv[1]
    out_fname = sys.argv[2]
    with open(in_fname, 'rb') as in_f, open(out_fname, 'r+b') as out_f:
        # Copy the main save slots
        save_slot_1_start = 0xf00000
        save_slot_2_start = 0xf20000
        for i in range(14):
            copy_sector(in_f, out_f, SAVE_SECTOR_SIZE * i, save_slot_1_start + SAVE_SECTOR_SIZE * i)
            copy_sector(in_f, out_f, SAVE_SECTOR_SIZE * (i + 14), save_slot_2_start + SAVE_SECTOR_SIZE * i)

        special_slots_start = 0xf40000
        flash_sector_size = 0x20000
        # Copy the other stuff
        for i in range(4):
            copy_sector(in_f, out_f, SAVE_SECTOR_SIZE * (i + 28), special_slots_start + flash_sector_size * i)
