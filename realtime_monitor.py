#!/usr/bin/env python3
"""
실시간 APU 트레이스 모니터
로그 파일을 실시간으로 읽으면서 특정 조건에서 일시정지
"""

import time
import sys
import os
import re
from collections import deque

class RealTimeMonitor:
    def __init__(self, log_file='apu_trace.log'):
        self.log_file = log_file
        self.breakpoints = {
            'pc': set(),
            'test': set(),
            'cycle': None,
            'port': set()
        }
        self.paused = False
        self.step_mode = False
        self.last_pc = None
        self.last_cycle = 0
        self.history = deque(maxlen=10)
        
    def add_breakpoint_pc(self, pc):
        """PC 주소에 브레이크포인트 추가"""
        self.breakpoints['pc'].add(pc)
        print(f"✓ Breakpoint set at PC:0x{pc:04X}")
    
    def add_breakpoint_test(self, test_num):
        """테스트 번호에 브레이크포인트 추가"""
        self.breakpoints['test'].add(test_num)
        print(f"✓ Breakpoint set on test 0x{test_num:02X}")
    
    def add_breakpoint_cycle(self, cycle):
        """사이클에 브레이크포인트 추가"""
        self.breakpoints['cycle'] = cycle
        print(f"✓ Breakpoint set at cycle {cycle}")
    
    def add_breakpoint_port(self, port):
        """포트 쓰기에 브레이크포인트 추가"""
        self.breakpoints['port'].add(port)
        print(f"✓ Breakpoint set on port {port} write")
    
    def parse_line(self, line):
        """로그 라인 파싱"""
        # [Cyc:0000875344] SPC700 PC:0x1306 | 8f 12 01 | MOV dp,#imm
        match = re.match(r'\[Cyc:(\d+)\] SPC700 PC:0x([0-9a-fA-F]+) \| ([0-9a-f ]+) \| (.+) \| A:0x([0-9a-fA-F]+) \| X:0x([0-9a-fA-F]+) \| Y:0x([0-9a-fA-F]+) \| SP:0x([0-9a-fA-F]+) \| PSW:0x([0-9a-fA-F]+)', line)
        
        if match:
            return {
                'cycle': int(match.group(1)),
                'pc': int(match.group(2), 16),
                'opcode': match.group(3),
                'instruction': match.group(4),
                'a': int(match.group(5), 16),
                'x': int(match.group(6), 16),
                'y': int(match.group(7), 16),
                'sp': int(match.group(8), 16),
                'psw': int(match.group(9), 16),
                'line': line.strip()
            }
        
        # 포트 쓰기: APU: SPC700 wrote port 2 = 0x41
        match = re.match(r'APU: SPC700 wrote port (\d) = 0x([0-9a-fA-F]+)', line)
        if match:
            return {
                'type': 'port_write',
                'port': int(match.group(1)),
                'value': int(match.group(2), 16),
                'line': line.strip()
            }
        
        return None
    
    def check_breakpoint(self, data):
        """브레이크포인트 체크"""
        if data is None:
            return False
        
        # 포트 쓰기 체크
        if data.get('type') == 'port_write':
            if data['port'] in self.breakpoints['port']:
                print(f"\n\n=== BREAKPOINT: Port {data['port']} write (value=0x{data['value']:02X}) ===")
                return True
            
            # 테스트 번호 (포트 2)
            if data['port'] == 2 and data['value'] in self.breakpoints['test']:
                print(f"\n\n=== BREAKPOINT: Test 0x{data['value']:02X} ===")
                return True
            
            return False
        
        # PC 브레이크포인트
        if data.get('pc') in self.breakpoints['pc']:
            print(f"\n\n=== BREAKPOINT: PC:0x{data['pc']:04X} ===")
            self.print_state(data)
            return True
        
        # 사이클 브레이크포인트
        if self.breakpoints['cycle'] and data.get('cycle', 0) >= self.breakpoints['cycle']:
            print(f"\n\n=== BREAKPOINT: Cycle {data['cycle']} ===")
            self.print_state(data)
            return True
        
        return False
    
    def print_state(self, data):
        """현재 상태 출력"""
        if data.get('type') == 'port_write':
            print(f"Port {data['port']} write: 0x{data['value']:02X}")
            return
        
        print(f"[Cyc:{data['cycle']:010d}] PC:0x{data['pc']:04X}")
        print(f"  A:0x{data['a']:02X} X:0x{data['x']:02X} Y:0x{data['y']:02X} SP:0x{data['sp']:02X} PSW:0x{data['psw']:02X}")
        print(f"  Instruction: {data['instruction']}")
        print(f"  Opcode: {data['opcode']}")
    
    def show_history(self):
        """최근 히스토리 출력"""
        print("\n=== Recent History ===")
        for i, data in enumerate(self.history, 1):
            if data.get('type') == 'port_write':
                print(f"{i}. Port {data['port']} = 0x{data['value']:02X}")
            else:
                print(f"{i}. [Cyc:{data['cycle']:010d}] PC:0x{data['pc']:04X} | {data['instruction']}")
    
    def interactive_prompt(self):
        """Interactive 프롬프트"""
        print("\n>>> Paused (type 'h' for help)")
        
        while self.paused:
            try:
                cmd = input("(monitor) ").strip().lower()
                
                if cmd in ['s', 'step']:
                    self.step_mode = True
                    self.paused = False
                    print("Stepping...")
                    
                elif cmd in ['c', 'continue']:
                    self.step_mode = False
                    self.paused = False
                    print("Continuing...")
                    
                elif cmd.startswith('n '):
                    try:
                        count = int(cmd.split()[1])
                        print(f"Stepping {count} instructions...")
                        # TODO: implement multi-step
                        self.step_mode = True
                        self.paused = False
                    except:
                        print("Usage: n <count>")
                
                elif cmd.startswith('b '):
                    try:
                        addr = int(cmd.split()[1], 16)
                        self.add_breakpoint_pc(addr)
                    except:
                        print("Usage: b <hex_address>")
                
                elif cmd.startswith('bt '):
                    try:
                        test = int(cmd.split()[1], 16)
                        self.add_breakpoint_test(test)
                    except:
                        print("Usage: bt <hex_test_number>")
                
                elif cmd.startswith('bp '):
                    try:
                        port = int(cmd.split()[1])
                        self.add_breakpoint_port(port)
                    except:
                        print("Usage: bp <port_number>")
                
                elif cmd in ['hist', 'history']:
                    self.show_history()
                
                elif cmd in ['h', 'help']:
                    self.show_help()
                
                elif cmd in ['q', 'quit']:
                    print("Quitting...")
                    sys.exit(0)
                
                else:
                    print(f"Unknown command: {cmd}")
                    print("Type 'h' for help")
                    
            except EOFError:
                break
            except KeyboardInterrupt:
                print("\nUse 'q' to quit")
    
    def show_help(self):
        """도움말 출력"""
        print("""
=== Monitor Commands ===
s / step       - Step one instruction
c / continue   - Continue until next breakpoint
n <count>      - Step N instructions
b <addr>       - Set breakpoint at PC (hex)
bt <test>      - Set breakpoint at test number (hex)
bp <port>      - Set breakpoint on port write
hist           - Show recent history
h / help       - Show this help
q / quit       - Quit monitor
""")
    
    def monitor(self):
        """메인 모니터링 루프"""
        print("=== Real-Time APU Monitor ===")
        print(f"Monitoring: {self.log_file}")
        print("Waiting for log file...")
        
        # 로그 파일이 생성될 때까지 대기
        while not os.path.exists(self.log_file):
            time.sleep(0.1)
        
        print("Log file found. Monitoring started.")
        print("Press Ctrl+C to stop\n")
        
        with open(self.log_file, 'r') as f:
            # 파일 끝으로 이동
            f.seek(0, 2)
            
            try:
                while True:
                    line = f.readline()
                    
                    if not line:
                        time.sleep(0.01)  # 짧은 대기
                        continue
                    
                    # 라인 파싱
                    data = self.parse_line(line)
                    
                    if data:
                        self.history.append(data)
                        
                        # 브레이크포인트 체크
                        if self.check_breakpoint(data) or self.step_mode:
                            self.paused = True
                            self.interactive_prompt()
                    
            except KeyboardInterrupt:
                print("\n\nMonitoring stopped.")

def main():
    if len(sys.argv) > 1:
        log_file = sys.argv[1]
    else:
        log_file = 'apu_trace.log'
    
    monitor = RealTimeMonitor(log_file)
    
    # 기본 브레이크포인트 설정 (optional)
    if len(sys.argv) > 2:
        if sys.argv[2] == '--test':
            test_num = int(sys.argv[3], 16)
            monitor.add_breakpoint_test(test_num)
        elif sys.argv[2] == '--pc':
            pc = int(sys.argv[3], 16)
            monitor.add_breakpoint_pc(pc)
    
    monitor.monitor()

if __name__ == '__main__':
    main()










