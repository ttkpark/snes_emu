#!/usr/bin/env python3
"""
CPU Trace Log Analyzer
자동으로 cpu_trace.log를 분석하여:
1. 마지막으로 실행된 테스트 번호 찾기
2. tests-basic.map에서 PC 주소로 테스트 번호 찾기
3. tests-basic.inc에서 해당 테스트의 어셈블리 코드 찾기
4. cpu_trace.log의 어셈블리와 비교해서 차이점 분석
"""

import re
import sys
from pathlib import Path

def parse_map_file(map_file):
    """tests-basic.map 파일을 파싱하여 테스트 번호와 주소 매핑 반환"""
    test_map = {}
    with open(map_file, 'r') as f:
        for line in f:
            # test0000                  008294  LF 형식
            match = re.match(r'^test([0-9a-f]+)\s+([0-9A-Fa-f]+)', line)
            if match:
                test_num = int(match.group(1), 16)
                addr = int(match.group(2), 16)
                test_map[addr] = test_num
    return test_map

def find_test_for_pc(test_map, pc):
    """주어진 PC 주소가 속한 테스트 번호를 찾음"""
    # PC 주소가 속한 테스트를 찾기 위해 가장 가까운 이전 테스트를 찾음
    sorted_addrs = sorted(test_map.keys())
    last_addr = None
    last_test = None
    
    for addr in sorted_addrs:
        if addr > pc:
            break
        last_addr = addr
        last_test = test_map[addr]
    
    if last_addr is None:
        # PC가 첫 번째 테스트보다 작으면 첫 번째 테스트 반환
        if sorted_addrs:
            first_addr = sorted_addrs[0]
            first_test = test_map[first_addr]
            next_addr = sorted_addrs[1] if len(sorted_addrs) > 1 else None
            return first_test, first_addr, next_addr
        return None, None, None
    
    # 다음 테스트의 주소를 찾아 범위 확인
    idx = sorted_addrs.index(last_addr)
    if idx + 1 < len(sorted_addrs):
        next_addr = sorted_addrs[idx + 1]
        if pc < next_addr:
            return last_test, last_addr, next_addr
    
    return last_test, last_addr, None

def parse_trace_log(trace_file):
    """cpu_trace.log 파일을 파싱하여 마지막 실행된 PC 주소 찾기"""
    last_pc = None
    last_line = None
    
    with open(trace_file, 'r') as f:
        for line in f:
            # [Cyc:0000409878 F:0000] PC:0x0095c5 | 2D FF FF    | AND abs 형식
            match = re.search(r'PC:0x([0-9A-Fa-f]+)\s+\|', line)
            if match:
                last_pc = int(match.group(1), 16)
                last_line = line.strip()
    
    return last_pc, last_line

def find_test_assembly(inc_file, test_num):
    """tests-basic.inc에서 특정 테스트 번호의 어셈블리 코드 찾기"""
    test_name = f'test{test_num:04x}'
    in_test = False
    test_lines = []
    
    with open(inc_file, 'r') as f:
        for line in f:
            if f'{test_name}:' in line or f'{test_name}:' in line.lower():
                in_test = True
                test_lines.append(line)
                continue
            
            if in_test:
                # 다음 테스트가 시작되면 중단
                if re.match(r'^test[0-9a-f]+:', line, re.IGNORECASE):
                    break
                
                test_lines.append(line)
    
    return test_lines

def analyze_assembly_diff(trace_line, expected_asm):
    """트레이스 로그의 어셈블리와 예상 어셈블리를 비교"""
    # 트레이스에서 명령어 추출
    trace_match = re.search(r'\|\s+([0-9A-Fa-f\s]+)\s+\|\s+([A-Za-z0-9\s#(),$]+)', trace_line)
    if not trace_match:
        return None
    
    trace_bytes = trace_match.group(1).strip()
    trace_instr = trace_match.group(2).strip()
    
    # 예상 어셈블리에서 해당 위치의 명령어 찾기
    # 간단한 비교만 수행
    return {
        'trace_bytes': trace_bytes,
        'trace_instr': trace_instr,
        'expected': expected_asm
    }

def main():
    base_dir = Path('.')
    map_file = base_dir / 'cputest' / 'tests-basic.map'
    inc_file = base_dir / 'cputest' / 'tests-basic.inc'
    trace_file = base_dir / 'cpu_trace.log'
    
    if not map_file.exists():
        print(f"Error: {map_file} not found")
        return
    
    if not inc_file.exists():
        print(f"Error: {inc_file} not found")
        return
    
    if not trace_file.exists():
        print(f"Error: {trace_file} not found")
        return
    
    # 맵 파일 파싱
    print("Parsing map file...")
    test_map = parse_map_file(map_file)
    print(f"Found {len(test_map)} tests in map file")
    
    # 트레이스 로그 분석
    print("Analyzing trace log...")
    last_pc, last_line = parse_trace_log(trace_file)
    if last_pc is None:
        print("Error: Could not find PC in trace log")
        return
    
    print(f"Last PC: 0x{last_pc:06x}")
    print(f"Last line: {last_line}")
    
    # 테스트 번호 찾기
    test_num, test_start, test_end = find_test_for_pc(test_map, last_pc)
    print(f"\nTest number: test{test_num:04x}")
    print(f"Test start: 0x{test_start:06x}")
    if test_end:
        print(f"Test end: 0x{test_end:06x}")
    
    # 어셈블리 코드 찾기
    print(f"\nFinding assembly for test{test_num:04x}...")
    test_asm = find_test_assembly(inc_file, test_num)
    
    if test_asm:
        print(f"\nAssembly code (first 30 lines):")
        for i, line in enumerate(test_asm[:30]):
            print(f"  {i+1:3d}: {line.rstrip()}")
    else:
        print("Could not find assembly code")
    
    # 차이점 분석
    print(f"\nAnalyzing difference...")
    diff = analyze_assembly_diff(last_line, test_asm)
    if diff:
        print(f"Trace instruction: {diff['trace_instr']}")
        print(f"Trace bytes: {diff['trace_bytes']}")
    
    return test_num, last_pc, last_line

if __name__ == '__main__':
    main()

