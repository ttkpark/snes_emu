@echo off
REM Interactive Debug 모드로 빌드

REM 프로젝트 루트 디렉토리로 이동
cd /d "%~dp0"

echo Building with Interactive Debugger...
echo.

REM Visual Studio 환경 설정 (Community 또는 Professional 버전 시도)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Error: Visual Studio vcvars64.bat not found!
    echo Please install Visual Studio 2019 or 2022.
    pause
    exit /b 1
)

REM 영어 메시지 설정
set VSLANG=1033

REM 소스 파일
set SOURCES=^
src\main_complete.cpp ^
src\cpu\cpu.cpp ^
src\memory\memory.cpp ^
src\ppu\ppu.cpp ^
src\apu\apu.cpp ^
src\input\simple_input.cpp ^
src\emulation\rom\rom_loader.cpp ^
src\emulation\rom\rom_mapper.cpp ^
src\io\video\video_output.cpp ^
src\io\audio\audio_output.cpp ^
src\debug\logger.cpp ^
src\debug\simple_debugger.cpp

REM 컴파일
cl /EHsc /std:c++17 /W3 ^
/D_CRT_SECURE_NO_WARNINGS ^
/DUSE_SDL ^
/DENABLE_INTERACTIVE_DEBUG ^
/DENABLE_LOGGING ^
/I. /Iinclude ^
/Ilib ^
%SOURCES% ^
/Fe:snes_emu_complete.exe ^
/link ^
lib\SDL2.lib lib\SDL2main.lib shell32.lib ^
/SUBSYSTEM:CONSOLE

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo.
    echo Executable: snes_emu_debug.exe
    echo.
    echo Usage:
    echo   snes_emu_debug.exe spctest.sfc
    echo.
    echo Interactive commands will be available
    echo during execution.
    echo ========================================
) else (
    echo.
    echo Build failed!
    pause
)










