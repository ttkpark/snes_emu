@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cl /EHsc test_rom_load.cpp >nul 2>&1
test_rom_load.exe

