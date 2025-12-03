#!/usr/bin/env python3
# IPL ROM 디스어셈블

ipl = [
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
]

# SPC700 명령어 디스어셈블
def disassemble(ipl, start_addr=0xFFC0):
    addr = start_addr
    i = 0
    while i < len(ipl):
        print(f'{hex(addr):>6}: ', end='')
        op = ipl[i]
        
        # 명령어 디스어셈블
        if op == 0xCD:  # MOV X, #imm
            print(f'MOV X, #${hex(ipl[i+1])}')
            i += 2
        elif op == 0xEF:  # MOV A, (X)+
            print(f'MOV A, (X)+')
            i += 1
        elif op == 0xBD:  # MOV SP, X
            print(f'MOV SP, X')
            i += 1
        elif op == 0xE8:  # MOV A, #imm
            print(f'MOV A, #${hex(ipl[i+1])}')
            i += 2
        elif op == 0x00:  # NOP
            print(f'NOP')
            i += 1
        elif op == 0xC6:  # MOV (X), A
            print(f'MOV (X), A')
            i += 1
        elif op == 0x1D:  # DEC X
            print(f'DEC X')
            i += 1
        elif op == 0xD0:  # BNE rel
            rel = ipl[i+1]
            if rel < 0x80:
                target = addr + 2 + rel
            else:
                target = addr + 2 + rel - 256
            print(f'BNE ${hex(target)}  ; rel=${hex(rel)}')
            i += 2
        elif op == 0xFC:  # INC Y
            print(f'INC Y')
            i += 1
        elif op == 0x8F:  # MOV dp, #imm
            print(f'MOV ${hex(ipl[i+1])}, #${hex(ipl[i+2])}')
            i += 3
        elif op == 0xCC:  # DEC A
            print(f'DEC A')
            i += 1
        elif op == 0xF4:  # MOV A, dp+X
            print(f'MOV A, dp+X')
            i += 1
        elif op == 0xFB:  # MOV Y, dp+X
            print(f'MOV Y, dp+X')
            i += 1
        elif op == 0x2F:  # BRA rel
            rel = ipl[i+1]
            if rel < 0x80:
                target = addr + 2 + rel
            else:
                target = addr + 2 + rel - 256
            print(f'BRA ${hex(target)}  ; rel=${hex(rel)}')
            i += 2
        elif op == 0x19:  # OR (X), (Y)
            print(f'OR (X), (Y)')
            i += 1
        elif op == 0xEB:  # MOV Y, dp
            print(f'MOV Y, ${hex(ipl[i+1])}  ; Y = port 0 ($F4)')
            i += 2
        elif op == 0x7E:  # CMP Y, dp
            print(f'CMP Y, ${hex(ipl[i+1])}  ; Compare Y with port 0 ($F4)')
            i += 2
        elif op == 0x0B:  # ASL dp
            print(f'ASL ${hex(ipl[i+1])}')
            i += 2
        elif op == 0xE4:  # MOV A, dp
            print(f'MOV A, ${hex(ipl[i+1])}')
            i += 2
        elif op == 0xCB:  # MOV Y, dp
            print(f'MOV Y, ${hex(ipl[i+1])}')
            i += 2
        elif op == 0xD7:  # MOV (dp)+Y, A
            print(f'MOV (dp)+Y, A')
            i += 1
        elif op == 0xF3:  # BBC dp.bit, rel
            print(f'BBC ${hex(ipl[i+1])}.bit, rel')
            i += 2
        elif op == 0xAB:  # INC dp
            print(f'INC ${hex(ipl[i+1])}')
            i += 2
        elif op == 0x01:  # TCALL 0
            print(f'TCALL 0')
            i += 1
        elif op == 0x10:  # BPL rel
            rel = ipl[i+1]
            if rel < 0x80:
                target = addr + 2 + rel
            else:
                target = addr + 2 + rel - 256
            print(f'BPL ${hex(target)}  ; rel=${hex(rel)}')
            i += 2
        elif op == 0xBA:  # MOVW YA, dp
            print(f'MOVW YA, dp')
            i += 2
        elif op == 0xF6:  # MOV A, (dp)+Y
            print(f'MOV A, (dp)+Y')
            i += 2
        elif op == 0xDA:  # MOVW dp, YA
            print(f'MOVW dp, YA')
            i += 2
        elif op == 0xC4:  # MOV dp, A
            print(f'MOV ${hex(ipl[i+1])}, A  ; Write A to port 0 ($F4) - ECHO!')
            i += 2
        elif op == 0xDD:  # MOV A, Y
            print(f'MOV A, Y')
            i += 1
        elif op == 0x5D:  # MOV X, A
            print(f'MOV X, A')
            i += 1
        elif op == 0xDB:  # MOV (dp)+Y, X
            print(f'MOV (dp)+Y, X')
            i += 1
        elif op == 0x1F:  # JMP (abs+X)
            print(f'JMP (abs+X)')
            i += 3
        else:
            print(f'UNKNOWN ${hex(op)}')
            i += 1
        
        addr = start_addr + i

disassemble(ipl)

