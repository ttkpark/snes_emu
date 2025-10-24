@echo off
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
cl /EHsc /std:c++17 /DUSE_SDL /I. /Iinclude src\main_complete.cpp src\cpu\cpu.cpp src\memory\memory.cpp src\ppu\ppu.cpp src\apu\apu.cpp src\input\simple_input.cpp src\debug\logger.cpp /Fe:snes_emu_complete.exe /link lib\SDL2.lib lib\SDL2main.lib shell32.lib /SUBSYSTEM:CONSOLE
pause
