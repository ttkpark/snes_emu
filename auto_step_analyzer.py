#!/usr/bin/env python3
"""
자동 Step 분석기
테스트 0x41~0x42 구간을 자동으로 step-by-step 분석
"""

import time
import sys
import os
import re
from collections import defaultdict

class AutoStepAnalyzer:
    def __init__(self, log_file='apu_trace.log'):
        self.log_file = log_file
        self.instructions = []
        self.current_test = 0x00
        self.test_ranges = {}  # test_num -> (start_idx, end_idx)
        self.anomalies = []
        
    def parse_instruction(self, line):
        """명령어 라인 파싱"""
        # [Cyc:0000875344] SPC700 PC:0x1306 | 8f 12 01 | MOV dp,#imm | A:0x41 | X:0x02 | Y:0x02 | SP:0xEF | PSW:0x03
        match = re.match(
            r'\[Cyc:(\d+)\] SPC700 PC:0x([0-9a-fA-F]+) \| ([0-9a-f ]+) \| (.+?) \| A:0x([0-9a-fA-F]+) \| X:0x([0-9a-fA-F]+) \| Y:0x([0-9a-fA-F]+) \| SP:0x([0-9a-fA-F]+) \| PSW:0x([0-9a-fA-F]+)',
            line
        )
        
        if match:
            return {
                'cycle': int(match.group(1)),
                'pc': int(match.group(2), 16),
                'opcode': match.group(3).strip(),
                'instruction': match.group(4).strip(),
                'a': int(match.group(5), 16),
                'x': int(match.group(6), 16),
                'y': int(match.group(7), 16),
                'sp': int(match.group(8), 16),
                'psw': int(match.group(9), 16),
            }
        return None
    
    def parse_port_write(self, line):
        """포트 쓰기 파싱"""
        match = re.match(r'APU: SPC700 wrote port (\d) = 0x([0-9a-fA-F]+)', line)
        if match:
            port = int(match.group(1))
            value = int(match.group(2), 16)
            if port == 2:  # 테스트 번호
                return value
        return None
    
    def load_log(self):
        """로그 파일 로드"""
        print(f"로그 파일 로드 중: {self.log_file}")
        
        if not os.path.exists(self.log_file):
            print("ERROR: 로그 파일이 없습니다. 에뮬레이터를 먼저 실행하세요.")
            return False
        
        with open(self.log_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        print(f"총 {len(lines)}줄 읽음")
        
        current_test = 0x00
        start_idx = 0
        
        for idx, line in enumerate(lines):
            # 테스트 번호 업데이트
            test_num = self.parse_port_write(line)
            if test_num is not None and test_num != current_test:
                if current_test in self.test_ranges:
                    self.test_ranges[current_test] = (self.test_ranges[current_test][0], idx - 1)
                
                current_test = test_num
                self.test_ranges[current_test] = (idx, None)
                print(f"  테스트 0x{test_num:02X} 발견 (line {idx})")
            
            # 명령어 파싱
            instr = self.parse_instruction(line)
            if instr:
                instr['test_num'] = current_test
                instr['line_num'] = idx
                self.instructions.append(instr)
        
        # 마지막 테스트 범위 종료
        if current_test in self.test_ranges and self.test_ranges[current_test][1] is None:
            self.test_ranges[current_test] = (self.test_ranges[current_test][0], len(lines) - 1)
        
        print(f"총 {len(self.instructions)}개 명령어 파싱")
        print(f"테스트 범위: {min(self.test_ranges.keys()):02X} ~ {max(self.test_ranges.keys()):02X}")
        return True
    
    def analyze_test(self, test_num):
        """특정 테스트 분석"""
        print(f"\n{'='*80}")
        print(f"테스트 0x{test_num:02X} 분석")
        print('='*80)
        
        # 해당 테스트의 명령어들 추출
        test_instrs = [i for i in self.instructions if i['test_num'] == test_num]
        
        if not test_instrs:
            print(f"테스트 0x{test_num:02X}의 명령어를 찾을 수 없습니다.")
            return
        
        print(f"총 {len(test_instrs)}개 명령어")
        print(f"사이클 범위: {test_instrs[0]['cycle']} ~ {test_instrs[-1]['cycle']}")
        print(f"PC 범위: 0x{test_instrs[0]['pc']:04X} ~ 0x{test_instrs[-1]['pc']:04X}")
        
        # PC 빈도 분석
        pc_freq = defaultdict(int)
        for instr in test_instrs:
            pc_freq[instr['pc']] += 1
        
        # 가장 많이 실행된 PC (무한 루프 의심)
        loops = [(pc, count) for pc, count in pc_freq.items() if count > 10]
        if loops:
            print(f"\n⚠️  무한 루프 의심 (10회 이상 실행):")
            loops.sort(key=lambda x: x[1], reverse=True)
            for pc, count in loops[:5]:
                print(f"  PC:0x{pc:04X} - {count}회")
        
        # 처음 20개 명령어 출력
        print(f"\n처음 20개 명령어:")
        for i, instr in enumerate(test_instrs[:20], 1):
            print(f"{i:3d}. [Cyc:{instr['cycle']:010d}] PC:0x{instr['pc']:04X} | {instr['instruction']:<20} | A:0x{instr['a']:02X} X:0x{instr['x']:02X} Y:0x{instr['y']:02X} PSW:0x{instr['psw']:02X}")
        
        # 이상 패턴 감지
        self.detect_anomalies_in_test(test_instrs, test_num)
    
    def detect_anomalies_in_test(self, instrs, test_num):
        """테스트 내 이상 패턴 감지"""
        print(f"\n🔍 이상 패턴 감지:")
        
        anomalies = []
        
        # 1. 같은 PC에서 다른 opcode (자기 수정 코드 또는 잘못된 매핑)
        pc_opcodes = defaultdict(set)
        for instr in instrs:
            pc_opcodes[instr['pc']].add(instr['opcode'])
        
        for pc, opcodes in pc_opcodes.items():
            if len(opcodes) > 1:
                anomalies.append({
                    'type': 'MULTIPLE_OPCODES',
                    'pc': pc,
                    'opcodes': list(opcodes),
                    'severity': 'HIGH'
                })
        
        # 2. 비정상적인 점프 (0x0000 또는 0xFFFF)
        for instr in instrs:
            if instr['pc'] == 0x0000 or instr['pc'] == 0xFFFF:
                anomalies.append({
                    'type': 'INVALID_PC',
                    'pc': instr['pc'],
                    'cycle': instr['cycle'],
                    'severity': 'CRITICAL'
                })
        
        # 3. 스택 포인터 이상 (underflow/overflow)
        for i in range(1, len(instrs)):
            prev_sp = instrs[i-1]['sp']
            curr_sp = instrs[i]['sp']
            sp_diff = curr_sp - prev_sp
            
            if abs(sp_diff) > 10:  # 한 번에 10바이트 이상 변화
                anomalies.append({
                    'type': 'SP_ANOMALY',
                    'pc': instrs[i]['pc'],
                    'prev_sp': prev_sp,
                    'curr_sp': curr_sp,
                    'diff': sp_diff,
                    'severity': 'MEDIUM'
                })
        
        # 4. 무한 루프 (같은 PC 연속 100회)
        consecutive_pc = 1
        for i in range(1, len(instrs)):
            if instrs[i]['pc'] == instrs[i-1]['pc']:
                consecutive_pc += 1
                if consecutive_pc >= 100:
                    anomalies.append({
                        'type': 'INFINITE_LOOP',
                        'pc': instrs[i]['pc'],
                        'count': consecutive_pc,
                        'severity': 'HIGH'
                    })
                    break
            else:
                consecutive_pc = 1
        
        # 5. 예상치 못한 PSW 변화 (특정 명령어가 PSW를 변경해야 하는데 안함)
        for i in range(1, len(instrs)):
            instr = instrs[i]
            prev_instr = instrs[i-1]
            
            # CMP 명령어는 PSW를 변경해야 함
            if 'CMP' in prev_instr['instruction'] or 'cmp' in prev_instr['instruction'].lower():
                if prev_instr['psw'] == instr['psw']:
                    # PSW가 변경되지 않음 (의심)
                    pass  # 너무 많을 수 있으므로 생략
        
        # 출력
        if anomalies:
            print(f"  발견된 이상: {len(anomalies)}개")
            for anom in anomalies[:10]:  # 최대 10개만 출력
                print(f"  [{anom['severity']}] {anom['type']}")
                for key, value in anom.items():
                    if key not in ['type', 'severity']:
                        if isinstance(value, int) and value > 255:
                            print(f"    {key}: 0x{value:04X}")
                        elif isinstance(value, int):
                            print(f"    {key}: 0x{value:02X}")
                        else:
                            print(f"    {key}: {value}")
        else:
            print("  이상 없음 ✓")
        
        self.anomalies.extend(anomalies)
    
    def compare_with_source(self, test_num):
        """소스 코드와 비교 (test0065 = 0x41)"""
        if test_num != 0x41:
            return
        
        print(f"\n📖 소스 코드 비교 (test0065 = 0x{test_num:02X}):")
        
        # 예상되는 명령어 시퀀스 (spctest/spc_tests0.asm에서)
        expected = [
            ("CALL init_test", "초기화"),
            ("MOV $01,#$7F", "메모리 초기화"),
            ("MOV A,#$00", "A 초기화"),
            ("PUSH A", "PSW 저장"),
            ("MOV A,#$12", "A = 0x12"),
            ("MOV X,#$34", "X = 0x34"),
            ("MOV Y,#$56", "Y = 0x56"),
            ("POP PSW", "PSW 복원"),
            ("CMP $01,#$3F", "비교"),
        ]
        
        test_instrs = [i for i in self.instructions if i['test_num'] == test_num]
        
        if not test_instrs:
            print("  명령어를 찾을 수 없습니다.")
            return
        
        print("\n  예상 vs 실제:")
        for i, (expected_instr, desc) in enumerate(expected):
            if i < len(test_instrs):
                actual = test_instrs[i]['instruction']
                match = "✓" if expected_instr.lower() in actual.lower() else "✗"
                print(f"  {i+1}. {match} 예상: {expected_instr:<20} | 실제: {actual:<20} ({desc})")
            else:
                print(f"  {i+1}. ? 예상: {expected_instr:<20} | 실제: (없음)")
    
    def analyze_all_tests(self):
        """모든 테스트 분석"""
        if not self.load_log():
            return
        
        # 마지막 테스트 찾기
        max_test = max(self.test_ranges.keys())
        print(f"\n마지막 테스트: 0x{max_test:02X}")
        
        # 0x41, 0x42 (마지막) 분석
        for test_num in [0x40, 0x41, max_test]:
            if test_num in self.test_ranges:
                self.analyze_test(test_num)
                if test_num == 0x41:
                    self.compare_with_source(test_num)
        
        # 전체 요약
        print(f"\n{'='*80}")
        print("전체 요약")
        print('='*80)
        print(f"분석한 테스트: {len(self.test_ranges)}개")
        print(f"테스트 범위: 0x{min(self.test_ranges.keys()):02X} ~ 0x{max(self.test_ranges.keys()):02X}")
        print(f"총 명령어: {len(self.instructions)}개")
        print(f"발견된 이상: {len(self.anomalies)}개")
        
        if self.anomalies:
            print(f"\n⚠️  심각도별 이상:")
            severity_count = defaultdict(int)
            for anom in self.anomalies:
                severity_count[anom['severity']] += 1
            
            for severity in ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW']:
                if severity in severity_count:
                    print(f"  {severity}: {severity_count[severity]}개")

def main():
    analyzer = AutoStepAnalyzer()
    analyzer.analyze_all_tests()

if __name__ == '__main__':
    main()










