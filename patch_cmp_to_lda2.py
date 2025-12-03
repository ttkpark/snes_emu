#!/usr/bin/env python3
"""Patch spctest.sfc: Change CMP $2140 to LDA $2140 at offset 0x018b"""

import sys
import os

def patch_rom():
    rom_file = 'spctest.sfc'
    
    if not os.path.exists(rom_file):
        print(f"Error: {rom_file} not found")
        return False
    
    # Read ROM file
    with open(rom_file, 'rb') as f:
        data = bytearray(f.read())
    
    # Patch offset 0x018b (CPU address 0x818b)
    offset = 0x018b
    
    if offset >= len(data):
        print(f"Error: Offset 0x{offset:04X} is out of range (file size: 0x{len(data):04X})")
        return False
    
    print(f"\nBefore patch at offset 0x{offset:04X}:")
    print(f"  Byte: 0x{data[offset]:02X}")
    print(f"  Next: 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
    
    if data[offset] != 0xCD:
        print(f"Warning: Byte at offset 0x{offset:04X} is 0x{data[offset]:02X}, not 0xCD")
        print("This might not be the correct location.")
    else:
        # Patch the byte
        data[offset] = 0xAD
        
        print(f"\nAfter patch at offset 0x{offset:04X}:")
        print(f"  Byte: 0x{data[offset]:02X} (changed to 0xAD)")
        print(f"  Next: 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
        
        # Write back
        with open(rom_file, 'wb') as f:
            f.write(data)
        
        print(f"\nSuccessfully patched {rom_file} at offset 0x{offset:04X}")
        print(f"Changed CMP $2140 (0xCD 0x40 0x21) to LDA $2140 (0xAD 0x40 0x21)")
        return True
    
    return False

if __name__ == '__main__':
    success = patch_rom()
    sys.exit(0 if success else 1)

