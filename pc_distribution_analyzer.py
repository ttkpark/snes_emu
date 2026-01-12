#!/usr/bin/env python3
"""
PC 분포 시각화 - SPC700이 어디서 실행되는지 한눈에 보기
"""

import re
from collections import defaultdict

def analyze_pc_distribution(log_file='apu_trace.log'):
    print("=" * 80)
    print("SPC700 PC 분포 분석")
    print("=" * 80)
    
    pc_ranges = {
        'IPL_ROM': (0xFFC0, 0xFFFF),
        'PROGRAM': (0x0000, 0xFFBF),
        'STACK': (0x0100, 0x01FF),
        'IO_REGS': (0x00F0, 0x00FF),
        'LOW_RAM': (0x0000, 0x00EF),
        'HIGH_RAM': (0x0200, 0xFFBF),
    }
    
    pc_counts = defaultdict(int)
    total_instructions = 0
    
    print(f"\n로그 파일 읽는 중: {log_file}")
    
    with open(log_file, 'r', encoding='utf-8') as f:
        for line in f:
            match = re.search(r'SPC700 PC:0x([0-9a-fA-F]+)', line)
            if match:
                pc = int(match.group(1), 16)
                pc_counts[pc] += 1
                total_instructions += 1
    
    print(f"총 명령어: {total_instructions:,}개")
    print(f"고유 PC: {len(pc_counts)}개")
    
    # 범위별 통계
    print(f"\n{'=' * 80}")
    print("PC 범위별 실행 통계")
    print('=' * 80)
    
    range_stats = defaultdict(int)
    for pc, count in pc_counts.items():
        for range_name, (start, end) in pc_ranges.items():
            if start <= pc <= end:
                range_stats[range_name] += count
                break
    
    for range_name in ['IPL_ROM', 'PROGRAM', 'STACK', 'IO_REGS', 'LOW_RAM', 'HIGH_RAM']:
        count = range_stats[range_name]
        percentage = (count / total_instructions * 100) if total_instructions > 0 else 0
        bar_length = int(percentage / 2)
        bar = '#' * bar_length
        
        status = ""
        if range_name == 'IPL_ROM' and percentage > 90:
            status = " [STUCK IN IPL ROM!]"
        elif range_name == 'PROGRAM' and percentage > 0:
            status = " [Program executing]"
        
        start, end = pc_ranges[range_name]
        print(f"{range_name:12} [0x{start:04X}-0x{end:04X}]: {count:8,} ({percentage:5.1f}%) {bar}{status}")
    
    # 가장 많이 실행된 PC
    print(f"\n{'=' * 80}")
    print("TOP 20 가장 많이 실행된 PC")
    print('=' * 80)
    
    top_pcs = sorted(pc_counts.items(), key=lambda x: x[1], reverse=True)[:20]
    for i, (pc, count) in enumerate(top_pcs, 1):
        percentage = (count / total_instructions * 100)
        print(f"{i:2}. PC:0x{pc:04X} - {count:8,}회 ({percentage:5.1f}%)")
    
    # IPL ROM에 갇혔는지 확인
    print(f"\n{'=' * 80}")
    print("진단")
    print('=' * 80)
    
    ipl_percentage = (range_stats['IPL_ROM'] / total_instructions * 100) if total_instructions > 0 else 0
    program_percentage = (range_stats['PROGRAM'] / total_instructions * 100) if total_instructions > 0 else 0
    
    if ipl_percentage > 99:
        print("[CRITICAL] SPC700이 IPL ROM에 갇혀있습니다!")
        print("   프로그램이 전혀 로드되지 않았습니다.")
        print("   원인: CPU가 IPL 프로토콜을 올바르게 완료하지 않음")
        print()
        print("해결 방법:")
        print("  1. CPU가 Port 3에 0x00을 쓰는지 확인")
        print("  2. CPU가 Port 0에 실행 주소를 쓰는지 확인")
        print("  3. APU가 Port 0 쓰기를 감지하고 IPL ROM을 비활성화하는지 확인")
        return False
    
    elif ipl_percentage > 50:
        print("[WARNING] SPC700이 IPL ROM에서 대부분의 시간을 보냅니다.")
        print(f"   프로그램 실행: {program_percentage:.1f}%만")
        return False
    
    elif program_percentage > 50:
        print("[OK] SPC700이 프로그램을 실행하고 있습니다.")
        print(f"   프로그램 실행: {program_percentage:.1f}%")
        return True
    
    else:
        print("[UNKNOWN] PC 분포가 이상합니다.")
        return False

if __name__ == '__main__':
    analyze_pc_distribution()










