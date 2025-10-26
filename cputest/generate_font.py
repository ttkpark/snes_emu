#!/usr/bin/env python3
"""
Generate a simple 8x8 ASCII font for SNES (2bpp format)
Each character is 16 bytes (8 rows, 2 bytes per row for 2bpp)
"""

def create_snes_font():
    # Create 2048 bytes (128 characters * 16 bytes)
    font_data = bytearray(2048)
    
    # Simple patterns for visible ASCII characters (0x20-0x7F)
    # For simplicity, we'll create basic patterns for digits and letters
    
    # Character '0' (0x30)
    char_0 = [
        0b01111100, 0b00000000,  # ****
        0b11000110, 0b00000000,  # **  **
        0b11001110, 0b00000000,  # **  ***
        0b11011110, 0b00000000,  # ** ****
        0b11110110, 0b00000000,  # **** **
        0b11100110, 0b00000000,  # ***  **
        0b11000110, 0b00000000,  # **  **
        0b01111100, 0b00000000,  # ****
    ]
    
    # Character '1' (0x31)
    char_1 = [
        0b00011000, 0b00000000,  #   **
        0b00111000, 0b00000000,  #  ***
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b01111110, 0b00000000,  # ******
    ]
    
    # Character '2' (0x32)
    char_2 = [
        0b01111100, 0b00000000,  # ****
        0b11000110, 0b00000000,  # **  **
        0b00000110, 0b00000000,  #     **
        0b00001100, 0b00000000,  #    **
        0b00110000, 0b00000000,  #   **
        0b01100000, 0b00000000,  #  **
        0b11000000, 0b00000000,  # **
        0b11111110, 0b00000000,  # *******
    ]
    
    # Character '3' (0x33)
    char_3 = [
        0b01111100, 0b00000000,  # ****
        0b11000110, 0b00000000,  # **  **
        0b00000110, 0b00000000,  #     **
        0b00111100, 0b00000000,  #  ****
        0b00000110, 0b00000000,  #     **
        0b00000110, 0b00000000,  #     **
        0b11000110, 0b00000000,  # **  **
        0b01111100, 0b00000000,  # ****
    ]
    
    # Character 'A' (0x41)
    char_A = [
        0b00111000, 0b00000000,  #  ***
        0b01101100, 0b00000000,  # ** **
        0b11000110, 0b00000000,  # **  **
        0b11000110, 0b00000000,  # **  **
        0b11111110, 0b00000000,  # *******
        0b11000110, 0b00000000,  # **  **
        0b11000110, 0b00000000,  # **  **
        0b11000110, 0b00000000,  # **  **
    ]
    
    # Character 'T' (0x54)
    char_T = [
        0b11111110, 0b00000000,  # *******
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
        0b00011000, 0b00000000,  #   **
    ]
    
    # Character 'E' (0x45)
    char_E = [
        0b11111110, 0b00000000,  # *******
        0b11000000, 0b00000000,  # **
        0b11000000, 0b00000000,  # **
        0b11111100, 0b00000000,  # ******
        0b11000000, 0b00000000,  # **
        0b11000000, 0b00000000,  # **
        0b11000000, 0b00000000,  # **
        0b11111110, 0b00000000,  # *******
    ]
    
    # Character 'S' (0x53)
    char_S = [
        0b01111100, 0b00000000,  # ****
        0b11000110, 0b00000000,  # **  **
        0b11000000, 0b00000000,  # **
        0b01111100, 0b00000000,  # ****
        0b00000110, 0b00000000,  #     **
        0b00000110, 0b00000000,  #     **
        0b11000110, 0b00000000,  # **  **
        0b01111100, 0b00000000,  # ****
    ]
    
    # Space (0x20)
    char_space = [0, 0] * 8
    
    # Insert characters into font data
    def insert_char(char_code, pattern):
        offset = char_code * 16
        for i, byte in enumerate(pattern):
            font_data[offset + i] = byte
    
    insert_char(0x20, char_space)  # Space
    insert_char(0x30, char_0)      # '0'
    insert_char(0x31, char_1)      # '1'
    insert_char(0x32, char_2)      # '2'
    insert_char(0x33, char_3)      # '3'
    insert_char(0x41, char_A)      # 'A'
    insert_char(0x45, char_E)      # 'E'
    insert_char(0x53, char_S)      # 'S'
    insert_char(0x54, char_T)      # 'T'
    
    # Add more basic characters (simple blocks for others)
    for c in range(0x34, 0x40):  # '4' to '@'
        offset = c * 16
        for i in range(8):
            font_data[offset + i*2] = 0xFF  # Simple filled pattern
            font_data[offset + i*2 + 1] = 0x00
    
    for c in range(0x42, 0x45):  # 'B', 'C', 'D'
        offset = c * 16
        for i in range(8):
            font_data[offset + i*2] = 0xFF
            font_data[offset + i*2 + 1] = 0x00
    
    for c in range(0x46, 0x53):  # 'F' to 'R'
        offset = c * 16
        for i in range(8):
            font_data[offset + i*2] = 0xFF
            font_data[offset + i*2 + 1] = 0x00
    
    for c in range(0x55, 0x80):  # 'U' to DEL
        offset = c * 16
        for i in range(8):
            font_data[offset + i*2] = 0xFF
            font_data[offset + i*2 + 1] = 0x00
    
    return bytes(font_data)

if __name__ == '__main__':
    font = create_snes_font()
    with open('font.bin', 'wb') as f:
        f.write(font)
    print(f"Generated font.bin: {len(font)} bytes")


