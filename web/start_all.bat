@echo off
echo [OK] SNES 웹 서버 시작...
start "SNES Web Server" cmd /c "node server.js"
timeout /t 2 /nobreak >nul
echo [OK] SSH 터널 연결 중...
start "SSH Tunnel" cmd /c "start_tunnel.bat"
echo.
echo [OK] 서버: http://localhost:3000
echo [OK] 원격: http://211.221.184.17:8006
echo.
pause
