#!/usr/bin/env python3
"""
Patch the existing spctest.sfc file with corrected SPC700 code
"""

# Read the existing ROM
with open('spctest.sfc', 'rb') as f:
    rom_data = bytearray(f.read())

print(f"Read {len(rom_data)} bytes from spctest.sfc")

# The SPC data should be somewhere in the ROM
# Let's search for the pattern "8F 00 F4 8F 00 F1"
pattern = bytes([0x8F, 0x00, 0xF4, 0x8F, 0x00, 0xF1])
offset = rom_data.find(pattern)

if offset == -1:
    print("ERROR: Could not find SPC code pattern in ROM!")
    exit(1)

print(f"Found SPC code at ROM offset 0x{offset:04X}")
print(f"Current bytes: {' '.join(f'{b:02X}' for b in rom_data[offset:offset+16])}")

# Generate the corrected SPC code
# main:
#     mov $f4, #$00
spc_code = bytearray([0x8F, 0x00, 0xF4])  # offset +0

#     mov $f1, #$00
spc_code.extend([0x8F, 0x00, 0xF1])  # offset +3

# .wait1:
#     cmp $f5, #$01
spc_code.extend([0x64, 0xF5, 0x01])  # offset +6 (FIXED!)

#     bne .wait1
spc_code.extend([0xD0, 0xFB])  # offset +9 (branch back -5 bytes)

#     mov a, $F6
spc_code.extend([0xE4, 0xF6])  # offset +11

#     mov test_num, a
spc_code.extend([0xC4, 0x10])  # offset +13

#     mov a, $F7
spc_code.extend([0xE4, 0xF7])  # offset +15

#     mov test_num+1, a
spc_code.extend([0xC4, 0x11])  # offset +17

#     jmp start_tests
spc_code.extend([0x5F, 0x59, 0x03])  # offset +19

# Replace the SPC code in the ROM
rom_data[offset:offset+len(spc_code)] = spc_code

print(f"Patched {len(spc_code)} bytes")
print(f"New bytes: {' '.join(f'{b:02X}' for b in rom_data[offset:offset+16])}")

# Write the patched ROM
with open('spctest.sfc', 'wb') as f:
    f.write(rom_data)

print(f"Wrote patched ROM to spctest.sfc")


