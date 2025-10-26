#!/usr/bin/env python3
font_data = bytearray(2048)

# Fill with 0xFF for testing
for i in range(2048):
    font_data[i] = 0xFF

with open('font.bin', 'wb') as f:
    f.write(font_data)

print(f"Generated font.bin with {len(font_data)} bytes")
print(f"First 16 bytes: {' '.join(f'{b:02X}' for b in font_data[:16])}")


