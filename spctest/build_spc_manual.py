#!/usr/bin/env python3
"""
Manually assemble the SPC700 code from spc_common.inc
This is a temporary solution until spcasm is available
"""

# SPC700 opcodes we need
opcodes = {
    'MOV_dp_imm': 0x8F,     # 8F imm dp
    'MOV_A_imm': 0xE8,      # E8 imm
    'MOV_Y_imm': 0x8D,      # 8D imm
    'CMP_dp_imm': 0x64,     # 64 dp imm  (CORRECTED from 0x68)
    'BNE_rel': 0xD0,        # D0 rel
    'CALL_abs': 0x3F,       # 3F addr_lo addr_hi
    'JMP_abs': 0x5F,        # 5F addr_lo addr_hi
    'RET': 0x6F,            # 6F
    'BRA_rel': 0x2F,        # 2F rel
    'INCW_dp': 0x3A,        # 3A dp
    'CMPW_YA_dp': 0x5A,     # 5A dp
    'BEQ_rel': 0xF0,        # F0 rel
    'MOVW_dp_YA': 0xDA,     # DA dp
    'MOV_A_dp': 0xE4,       # E4 dp
    'MOV_dp_A': 0xC4,       # C4 dp
    'PUSH_PSW': 0x0D,       # 0D
    'POP_A': 0xAE,          # AE
    'AND_A_imm': 0x28,      # 28 imm
}

# Start at 0x0300
spc_binary = bytearray()
addr = 0x0300

# main:
#     mov $f4, #$00
spc_binary.extend([0x8F, 0x00, 0xF4])  # addr 0x0300-0x0302

#     mov $f1, #$00  ; disable IPL ROM
spc_binary.extend([0x8F, 0x00, 0xF1])  # addr 0x0303-0x0305

# .wait1:  ; wait for CPU to set port1 = 1
#     cmp $f5, #$01
spc_binary.extend([0x64, 0xF5, 0x01])  # addr 0x0306-0x0308 (FIXED!)

#     bne .wait1
spc_binary.extend([0xD0, 0xFB])  # addr 0x0309-0x030A (branch back -5 bytes to 0x0306)

#     ; CPU put last test num in ports 2/3
#     mov a, $F6
spc_binary.extend([0xE4, 0xF6])  # addr 0x030B-0x030C

#     mov test_num, a    ; test_num at dp 0x10
spc_binary.extend([0xC4, 0x10])  # addr 0x030D-0x030E

#     mov a, $F7
spc_binary.extend([0xE4, 0xF7])  # addr 0x030F-0x0310

#     mov test_num+1, a
spc_binary.extend([0xC4, 0x11])  # addr 0x0311-0x0312

#     jmp start_tests   ; start_tests at 0x0359 (from spc_tests0.asm)
spc_binary.extend([0x5F, 0x59, 0x03])  # addr 0x0313-0x0315

print(f"Generated {len(spc_binary)} bytes of SPC700 code")
print("First 32 bytes (hex):")
for i in range(min(32, len(spc_binary))):
    if i % 16 == 0:
        print(f"\n{0x0300+i:04X}: ", end="")
    print(f"{spc_binary[i]:02X} ", end="")
print()

# Save to file (starting from 0x0300)
# But we need to pad from 0x0000 to 0x0300 first
full_binary = bytearray(0x0300) + spc_binary

# Continue adding the rest (init_test, save_results, success, fail)
# For now, just pad to make it long enough
while len(full_binary) < 0x0400:
    full_binary.append(0x00)

with open('spc_tests0_manual.spc', 'wb') as f:
    f.write(full_binary)
    
print(f"\nWrote {len(full_binary)} bytes to spc_tests0_manual.spc")


