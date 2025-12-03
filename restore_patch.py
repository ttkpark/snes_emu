#!/usr/bin/env python3
"""Restore spctest.sfc: Change LDA $2140 (0xAD 0x40 0x21) back to CMP $2140 (0xCD 0x40 0x21)"""

import sys
import os

def restore_rom():
    rom_file = 'spctest.sfc'
    backup_file = rom_file + '.bak'
    
    # Try to restore from backup first
    if os.path.exists(backup_file):
        print(f"Restoring from backup: {backup_file}")
        with open(backup_file, 'rb') as f:
            data = f.read()
        with open(rom_file, 'wb') as f:
            f.write(data)
        print(f"Successfully restored {rom_file} from backup")
        return True
    
    # If no backup, manually change 0xAD back to 0xCD
    if not os.path.exists(rom_file):
        print(f"Error: {rom_file} not found")
        return False
    
    # Read ROM file
    with open(rom_file, 'rb') as f:
        data = bytearray(f.read())
    
    # Restore offsets 0x0162 and 0x018b
    offsets_to_restore = [0x0162, 0x018b]
    restored = False
    
    for offset in offsets_to_restore:
        if offset < len(data):
            if data[offset] == 0xAD and data[offset+1] == 0x40 and data[offset+2] == 0x21:
                print(f"\nRestoring offset 0x{offset:04X}:")
                print(f"  Before: 0x{data[offset]:02X} 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
                data[offset] = 0xCD
                print(f"  After:  0x{data[offset]:02X} 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
                restored = True
            else:
                print(f"Offset 0x{offset:04X} doesn't match LDA $2140 pattern")
    
    if restored:
        # Write back
        with open(rom_file, 'wb') as f:
            f.write(data)
        print(f"\nSuccessfully restored {rom_file}")
        print(f"Changed LDA $2140 (0xAD 0x40 0x21) back to CMP $2140 (0xCD 0x40 0x21)")
        return True
    else:
        print("No changes needed - file already in original state")
        return True

if __name__ == '__main__':
    success = restore_rom()
    sys.exit(0 if success else 1)

