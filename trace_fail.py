#!/usr/bin/env python3
# -*- coding: utf-8 -*-

with open('cpu_trace.log', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# 0x8229 이전에 실행된 코드를 역순으로 찾기
found_bra = False
count = 0
for i in range(len(lines)-1, -1, -1):
    line = lines[i]
    if 'PC:0x008229' in line and 'BRA' in line:
        found_bra = True
        print(f"=== Found BRA @end at line {i+1} ===")
        print(f"{i+1}:{line.strip()}")
    elif found_bra:
        # 0x8126~0x8229 범위에서 wait_result 함수와 @failed 찾기
        if 'PC:0x008' in line:
            pc_str = line.split('PC:0x008')[1].split()[0]
            try:
                pc = int(pc_str, 16)
                if 0x8126 <= pc <= 0x8229:
                    # 중요한 명령어만 출력
                    if 'CMP' in line or 'BNE' in line or 'LDA' in line or 'JMP' in line or 'JSR' in line or 'BRA' in line:
                        print(f"{i+1}:{line.strip()}")
                        count += 1
                        if count > 100:  # 최대 100줄만 출력
                            break
                    # wait_result 함수의 CMP #$01과 BNE @failed 찾기
                    if 'CMP' in line and '#$01' in line:
                        print(f"  >>> FOUND CMP #$01 at line {i+1}")
                    if 'BNE' in line:
                        print(f"  >>> FOUND BNE at line {i+1}")
            except:
                pass
        # 0x8126 이전으로 가면 중단
        if 'PC:0x008' in line:
            try:
                pc_str = line.split('PC:0x008')[1].split()[0]
                pc = int(pc_str, 16)
                if pc < 0x8126:
                    break
            except:
                pass



