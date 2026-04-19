@echo off
setlocal enabledelayedexpansion

REM Setup Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" > /dev/null 2>&1

REM Compile
cl /EHsc /std:c++17 /O2 /DUSE_SDL /I. /Iinclude ^
  src\main_complete.cpp ^
  src\cpu\cpu.cpp ^
  src\memory\memory.cpp ^
  src\ppu\ppu.cpp ^
  src\apu\apu.cpp ^
  src\input\simple_input.cpp ^
  src\debug\logger.cpp ^
  /Fe:snes_emu_complete.exe ^
  /link lib\SDL2.lib lib\SDL2main.lib shell32.lib /SUBSYSTEM:CONSOLE

if exist snes_emu_complete.exe (
  echo BUILD SUCCESS
  exit /b 0
) else (
  echo BUILD FAILED
  exit /b 1
)
