@echo off
echo [TUNNEL] 원격 포트 8006 -^> 로컬 3000 터널 시작...
echo [TUNNEL] (내부 중계 포트: 28006)
ssh -p 39332 -i "%USERPROFILE%\.ssh\id_ed25519_face" ^
    -R 28006:127.0.0.1:3000 ^
    -N -o ServerAliveInterval=30 ^
    server@211.221.184.17
