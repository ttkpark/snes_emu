import subprocess
import time
from pathlib import Path
import os

cwd = os.getcwd()
exe_path = Path(cwd) / "snes_emu_complete.exe"
rom_path = Path(cwd) / "SNES Test Program.sfc"

print(f"CWD: {cwd}")
print(f"EXE: {exe_path} (exists: {exe_path.exists()})")
print(f"ROM: {rom_path} (exists: {rom_path.exists()})")

if exe_path.exists() and rom_path.exists():
    print(f"\n[1] 에뮬레이터 실행 중...")
    subprocess.Popen([str(exe_path), str(rom_path)])
    print("[2] 5초 대기 중...")
    time.sleep(5)
    
    result = subprocess.run(['tasklist'], capture_output=True, text=True)
    if 'snes_emu_complete.exe' in result.stdout:
        print("[3] ✅ 에뮬레이터 실행 확인됨")
    else:
        print("[3] ❌ 에뮬레이터 미실행")
else:
    print(f"❌ 파일 없음")
