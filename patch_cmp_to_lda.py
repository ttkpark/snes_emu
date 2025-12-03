#!/usr/bin/env python3
"""Patch spctest.sfc: Change CMP $2140 (0xCD 0x40 0x21) to LDA $2140 (0xAD 0x40 0x21) at offset 0x018b"""

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
    
    # LoROM mapping: CPU address 0x00818b maps to ROM offset 0x018b (0x818b - 0x8000)
    # But we need to account for SNES header (512 bytes = 0x200)
    # So actual offset might be 0x018b or 0x218b (with header)
    
    # First, try to find the pattern
    pattern = bytes([0xCD, 0x40, 0x21])  # CMP $2140
    replacement = bytes([0xAD, 0x40, 0x21])  # LDA $2140
    
    found = False
    offset = None
    
    # Search for the pattern
    for i in range(len(data) - 2):
        if data[i:i+3] == pattern:
            print(f"Found CMP $2140 at offset 0x{i:04X}")
            # Check if this is near the expected location (0x018b or 0x218b)
            if abs(i - 0x018b) < 0x100 or abs(i - 0x218b) < 0x100:
                offset = i
                found = True
                break
    
    if not found:
        # Try direct offset
        offsets_to_try = [0x018b, 0x218b, 0x418b]  # Without header, with header, alternative
        for off in offsets_to_try:
            if off < len(data) - 2:
                if data[off:off+3] == pattern:
                    offset = off
                    found = True
                    print(f"Found CMP $2140 at offset 0x{offset:04X} (direct)")
                    break
    
    if not found:
        print("Error: CMP $2140 pattern not found in ROM")
        print("Trying to patch at expected offset 0x018b anyway...")
        offset = 0x018b
        if offset >= len(data):
            print(f"Error: Offset 0x{offset:04X} is out of range (file size: 0x{len(data):04X})")
            return False
    
    # Verify the byte before patching
    if offset < len(data):
        print(f"\nBefore patch at offset 0x{offset:04X}:")
        print(f"  Byte: 0x{data[offset]:02X} (should be 0xCD)")
        print(f"  Next: 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
        
        if data[offset] != 0xCD:
            print(f"Warning: Byte at offset 0x{offset:04X} is 0x{data[offset]:02X}, not 0xCD")
            response = input("Continue anyway? (y/n): ")
            if response.lower() != 'y':
                return False
        
        # Patch the byte
        data[offset] = 0xAD
        
        print(f"\nAfter patch at offset 0x{offset:04X}:")
        print(f"  Byte: 0x{data[offset]:02X} (changed to 0xAD)")
        print(f"  Next: 0x{data[offset+1]:02X} 0x{data[offset+2]:02X}")
        
        # Write back
        backup_file = rom_file + '.bak'
        if not os.path.exists(backup_file):
            with open(backup_file, 'wb') as f:
                f.write(bytes(data))
            print(f"\nBackup created: {backup_file}")
        
        with open(rom_file, 'wb') as f:
            f.write(data)
        
        print(f"\nSuccessfully patched {rom_file}")
        print(f"Changed CMP $2140 (0xCD 0x40 0x21) to LDA $2140 (0xAD 0x40 0x21)")
        return True
    else:
        print(f"Error: Offset 0x{offset:04X} is out of range")
        return False

if __name__ == '__main__':
    success = patch_rom()
    sys.exit(0 if success else 1)

