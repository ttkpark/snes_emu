#!/usr/bin/env python3
# -*- coding: utf-8 -*-

with open('cpu_trace.log', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# 0x8229 이전에 실행된 코드를 역순으로 찾기
found_bra = False
for i in range(len(lines)-1, -1, -1):
    line = lines[i]
    if 'PC:0x008229' in line and 'BRA' in line:
        found_bra = True
        print(f"=== Found BRA @end at line {i+1} ===")
    elif found_bra:
        # 0x8126~0x8130 범위에서 wait_result 함수 찾기
        if 'PC:0x00812' in line:
            print(f"{i+1}:{line.strip()}")
            # wait_result 함수의 CMP #$01과 BNE @failed 찾기
            if 'CMP' in line and '#$01' in line:
                print(f"  >>> FOUND CMP #$01 at line {i+1}")
            if 'BNE' in line and 'PC:0x00812' in line:
                print(f"  >>> FOUND BNE @failed at line {i+1}")
        # 0x8126 이전으로 가면 중단
        if 'PC:0x00812' in line and int(line.split('PC:0x008')[1].split()[0], 16) < 0x126:
            break
        # 0x8130 이후로 가면 중단
        if 'PC:0x00813' in line and int(line.split('PC:0x008')[1].split()[0], 16) > 0x130:
            break
        # 100줄 이상 역추적했으면 중단
        if i < len(lines) - 200:
            break



