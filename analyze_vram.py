#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re

def analyze_vram_dump():
    with open('vram_dump.txt', 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    
    print("=" * 60)
    print("VRAM Dump Analysis")
    print("=" * 60)
    
    # Parse VRAM data
    vram = [0] * 65536
    for line in lines[2:]:  # Skip header
        m = re.search(r'^([0-9a-f]+):\s+([0-9a-f\s]+)', line)
        if m:
            addr = int(m.group(1), 16)
            hex_bytes = [int(b, 16) for b in m.group(2).split() if b]
            for i, byte in enumerate(hex_bytes):
                if addr + i < len(vram):
                    vram[addr + i] = byte
    
    # Analyze BG1 Tilemap (0x0000-0x07FF)
    print("\n1. BG1 Tilemap Analysis (0x0000-0x07FF, 2048 bytes)")
    print("-" * 60)
    tilemap_entries = []
    for i in range(0, 0x800, 2):
        entry = vram[i] | (vram[i + 1] << 8)
        if entry != 0:
            tile_num = entry & 0x3FF
            palette = (entry >> 10) & 0x7
            priority = (entry >> 13) & 1
            hflip = (entry >> 14) & 1
            vflip = (entry >> 15) & 1
            tilemap_entries.append((i, entry, tile_num, palette, priority, hflip, vflip))
    
    print(f"  Non-zero tilemap entries: {len(tilemap_entries)} / 1024")
    if len(tilemap_entries) > 0:
        print("  First 10 non-zero entries:")
        for addr, entry, tile, pal, pri, hf, vf in tilemap_entries[:10]:
            print(f"    Addr 0x{addr:04X}: Entry=0x{entry:04X} Tile={tile:3d} Pal={pal} Pri={pri} HFlip={hf} VFlip={vf}")
    else:
        print("  WARNING: All tilemap entries are zero!")
    
    # Analyze BG1 Tiles at 0x4000
    print("\n2. BG1 Tiles at 0x4000 (2bpp, 16 bytes/tile)")
    print("-" * 60)
    tiles_4000 = []
    for tile_idx in range(0, (0x8000 - 0x4000) // 16):
        tile_addr = 0x4000 + tile_idx * 16
        tile_data = vram[tile_addr:tile_addr + 16]
        if any(tile_data):
            tiles_4000.append((tile_idx, tile_addr, tile_data))
    
    print(f"  Tiles with data: {len(tiles_4000)} / {((0x8000 - 0x4000) // 16)}")
    if len(tiles_4000) > 0:
        print("  First 5 tiles with data:")
        for tile_idx, tile_addr, tile_data in tiles_4000[:5]:
            hex_str = ' '.join(f'{b:02x}' for b in tile_data)
            print(f"    Tile {tile_idx:3d} @ 0x{tile_addr:04X}: {hex_str}")
    else:
        print("  WARNING: No tile data at 0x4000!")
    
    # Analyze Font Tiles at 0x8210
    print("\n3. Font Tiles at 0x8210 (2bpp, 16 bytes/tile)")
    print("-" * 60)
    font_tiles = []
    for tile_idx in range(0, 100):  # Check first 100 tiles
        tile_addr = 0x8210 + tile_idx * 16
        if tile_addr + 16 > len(vram):
            break
        tile_data = vram[tile_addr:tile_addr + 16]
        if any(tile_data):
            font_tiles.append((tile_idx, tile_addr, tile_data))
    
    print(f"  Font tiles with data: {len(font_tiles)}")
    if len(font_tiles) > 0:
        print("  First 10 font tiles:")
        for tile_idx, tile_addr, tile_data in font_tiles[:10]:
            hex_str = ' '.join(f'{b:02x}' for b in tile_data)
            print(f"    Font Tile {tile_idx:3d} @ 0x{tile_addr:04X}: {hex_str}")
    
    # Check if tilemap points to valid tiles
    print("\n4. Tilemap to Tile Data Mapping")
    print("-" * 60)
    if len(tilemap_entries) > 0:
        print("  Checking if tilemap entries point to valid tiles...")
        bg1_tile_base = 0x4000  # BG12NBA = $4 (from PPU log)
        print(f"  BG1 tile base: 0x{bg1_tile_base:04X} (BG12NBA = $4)")
        print()
        
        tiles_found_at_base = 0
        tiles_found_at_font = 0
        tiles_not_found = 0
        
        for addr, entry, tile_num, pal, pri, hf, vf in tilemap_entries[:30]:
            # Calculate tile address based on BG1 tile base
            tile_addr_base = bg1_tile_base + tile_num * 16
            tile_addr_font = 0x8210 + tile_num * 16
            
            has_data_base = any(vram[tile_addr_base:tile_addr_base + 16]) if tile_addr_base + 16 <= len(vram) else False
            has_data_font = any(vram[tile_addr_font:tile_addr_font + 16]) if tile_addr_font + 16 <= len(vram) else False
            
            if has_data_base:
                tiles_found_at_base += 1
                status = "[OK] FOUND at base"
            elif has_data_font:
                tiles_found_at_font += 1
                status = "[OK] FOUND at font (0x8210)"
            else:
                tiles_not_found += 1
                status = "[FAIL] NOT FOUND"
            
            # Show tile data if found
            if has_data_base or has_data_font:
                tile_addr = tile_addr_base if has_data_base else tile_addr_font
                tile_data = vram[tile_addr:tile_addr + 16]
                hex_str = ' '.join(f'{b:02x}' for b in tile_data)
                print(f"    Tile {tile_num:3d} (map@0x{addr:04X}): {status}")
                print(f"      Base@0x{tile_addr_base:04X}: {'HAS DATA' if has_data_base else 'NO DATA'}")
                print(f"      Font@0x{tile_addr_font:04X}: {'HAS DATA' if has_data_font else 'NO DATA'}")
                print(f"      Data: {hex_str}")
            else:
                print(f"    Tile {tile_num:3d} (map@0x{addr:04X}): {status}")
        
        print()
        print(f"  Summary:")
        print(f"    Tiles found at base (0x{bg1_tile_base:04X}): {tiles_found_at_base}")
        print(f"    Tiles found at font (0x8210): {tiles_found_at_font}")
        print(f"    Tiles not found: {tiles_not_found}")
    else:
        print("  No tilemap entries to check!")
    
    # Overall statistics
    print("\n5. Overall VRAM Statistics")
    print("-" * 60)
    total_nonzero = sum(1 for b in vram if b != 0)
    print(f"  Total non-zero bytes: {total_nonzero} / {len(vram)} ({total_nonzero * 100 // len(vram)}%)")
    
    # Check key regions
    regions = [
        ("BG1 Tilemap", 0x0000, 0x0800),
        ("BG1 Tiles @0x4000", 0x4000, 0x8000),
        ("Font Tiles @0x8210", 0x8210, 0x9000),
    ]
    
    print("\n6. Key Region Summary")
    print("-" * 60)
    for name, start, end in regions:
        nonzero = sum(1 for i in range(start, min(end, len(vram))) if vram[i] != 0)
        total = end - start
        print(f"  {name:20s} (0x{start:04X}-0x{end:04X}): {nonzero:5d} / {total:5d} non-zero ({nonzero*100//total if total > 0 else 0}%)")

if __name__ == '__main__':
    analyze_vram_dump()

