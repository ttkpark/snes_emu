#!/usr/bin/env python3
# -*- coding: utf-8 -*-

with open('cpu_trace.log', 'r', encoding='utf-8') as f:
    lines = f.readlines()

found_bra = False
for i in range(len(lines)-1, -1, -1):
    line = lines[i]
    if 'PC:0x008229' in line and 'BRA' in line:
        found_bra = True
    elif found_bra and 'PC:0x008' in line:
        if 'CMP' in line or 'BNE' in line or 'LDA' in line or 'JMP' in line or 'JSR' in line:
            print(f'{i+1}:{line.strip()}')
        if 'PC:0x00812E' in line or 'PC:0x00812F' in line or 'PC:0x008130' in line or 'PC:0x008131' in line or 'PC:0x008132' in line or 'PC:0x008133' in line or 'PC:0x008134' in line:
            break



