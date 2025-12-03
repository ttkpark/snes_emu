#!/usr/bin/env python3
"""Restore spctest.sfc: Change LDA $2140 (0xAD) back to CMP $2140 (0xCD) at offset 0x0162"""

import sys
import os

def restore_rom():
    rom_file = 'spctest.sfc'
    
    if not os.path.exists(rom_file):
        print(f"Error: {rom_file} not found")
        return False
    
    # Read ROM file
    with open(rom_file, 'rb') as f:
        data = bytearray(f.read())
    
    # Restore offset 0x0162
    offset = 0x0162
    
    if offset >= len(data):
        print(f"Error: Offset 0x{offset:04X} is out of range")
        return False
    
    print(f"\nBefore restore at offset 0x{offset:04X}:")
    print(f"  Byte: 0x{data[offset]:02X}")
    print(f"  Next: 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
    
    if data[offset] == 0xAD and data[offset+1] == 0x40 and data[offset+2] == 0x21:
        # Restore to CMP
        data[offset] = 0xCD
        
        print(f"\nAfter restore at offset 0x{offset:04X}:")
        print(f"  Byte: 0x{data[offset]:02X} (changed to 0xCD)")
        print(f"  Next: 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
        
        # Write back
        with open(rom_file, 'wb') as f:
            f.write(data)
        
        print(f"\nSuccessfully restored {rom_file} at offset 0x{offset:04X}")
        print(f"Changed LDA $2140 (0xAD 0x40 0x21) back to CMP $2140 (0xCD 0x40 0x21)")
        return True
    else:
        print(f"Offset 0x{offset:04X} doesn't match LDA $2140 pattern (current: 0x{data[offset]:02X})")
        return False

if __name__ == '__main__':
    success = restore_rom()
    sys.exit(0 if success else 1)

