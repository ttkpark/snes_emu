param(
    [string]$LogFile = "port_comm.log",
    [int]$TailLines = 1000,
    [switch]$Verbose,
    [switch]$Timeline,
    [switch]$VerifyProtocol = $true
)

Write-Host "=== Port Communication Analyzer ===" -ForegroundColor Cyan
Write-Host "Log file: $LogFile"
Write-Host "Analyzing last $TailLines lines..."
Write-Host ""

# 로그 파일 확인
if (-not (Test-Path $LogFile)) {
    Write-Host "ERROR: Log file not found: $LogFile" -ForegroundColor Red
    exit 1
}

# 로그 읽기
$lines = Get-Content $LogFile -Tail $TailLines

# 이벤트 파싱
$events = @()
foreach ($line in $lines) {
    if ($line -match 'APU: (CPU|SPC700) (read|wrote) port (\d) = 0x([0-9a-f]+).*PC=0x([0-9a-f]+)') {
        $event = @{
            Source = $matches[1]
            Action = $matches[2]
            Port = [int]$matches[3]
            Value = [int]"0x$($matches[4])"
            PC = [int]"0x$($matches[5])"
            Line = $line
        }
        $events += $event
    }
}

Write-Host "Total port events: $($events.Count)" -ForegroundColor Green
Write-Host ""

# 통계
$cpuWrites = ($events | Where-Object { $_.Source -eq "CPU" -and $_.Action -eq "wrote" }).Count
$cpuReads = ($events | Where-Object { $_.Source -eq "CPU" -and $_.Action -eq "read" }).Count
$spcWrites = ($events | Where-Object { $_.Source -eq "SPC700" -and $_.Action -eq "wrote" }).Count
$spcReads = ($events | Where-Object { $_.Source -eq "SPC700" -and $_.Action -eq "read" }).Count

Write-Host "=== Statistics ===" -ForegroundColor Yellow
Write-Host "CPU writes: $cpuWrites"
Write-Host "CPU reads:  $cpuReads"
Write-Host "SPC writes: $spcWrites"
Write-Host "SPC reads:  $spcReads"
Write-Host ""

# 최종 포트 값
Write-Host "=== Current Port Values ===" -ForegroundColor Yellow
for ($port = 0; $port -le 3; $port++) {
    $lastWrite = $events | Where-Object { $_.Port -eq $port -and $_.Action -eq "wrote" } | Select-Object -Last 1
    if ($lastWrite) {
        $valueHex = $lastWrite.Value.ToString('X2')
        $pcHex = $lastWrite.PC.ToString('X4')
        Write-Host "Port ${port}: 0x${valueHex} (by $($lastWrite.Source) at PC:0x${pcHex})"
    } else {
        Write-Host "Port ${port}: No writes recorded"
    }
}
Write-Host ""

# 프로토콜 검증
if ($VerifyProtocol) {
    Write-Host "=== Protocol Verification (spctest) ===" -ForegroundColor Yellow
    
    # CPU가 Port 0에 0xCC를 썼는지
    $port0Init = $events | Where-Object { 
        $_.Source -eq "CPU" -and $_.Action -eq "wrote" -and $_.Port -eq 0 -and $_.Value -eq 0xCC 
    }
    
    if ($port0Init) {
        Write-Host "V CPU wrote 0xCC to Port 0" -ForegroundColor Green
    } else {
        Write-Host "X CPU did NOT write 0xCC to Port 0" -ForegroundColor Red
    }
    
    # CPU가 Port 1에 0x01을 썼는지
    $port1Init = $events | Where-Object { 
        $_.Source -eq "CPU" -and $_.Action -eq "wrote" -and $_.Port -eq 1 -and $_.Value -eq 0x01 
    }
    
    if ($port1Init) {
        Write-Host "V CPU wrote 0x01 to Port 1" -ForegroundColor Green
    } else {
        Write-Host "X CPU did NOT write 0x01 to Port 1" -ForegroundColor Red
    }
    
    # SPC가 Port 0에 응답했는지
    $spcResponse = $events | Where-Object { 
        $_.Source -eq "SPC700" -and $_.Action -eq "wrote" -and $_.Port -eq 0 -and $_.Value -eq 0x00 
    }
    
    if ($spcResponse) {
        Write-Host "V SPC responded with 0x00 to Port 0" -ForegroundColor Green
    } else {
        Write-Host "X SPC did NOT respond with 0x00 to Port 0" -ForegroundColor Red
    }
    
    # SPC가 fail 루틴에 진입했는지
    $spcFailed = $events | Where-Object { 
        $_.Source -eq "SPC700" -and $_.Action -eq "wrote" -and $_.Port -eq 0 -and 
        ($_.Value -eq 0x02 -or $_.Value -eq 0x03)
    }
    
    if ($spcFailed) {
        Write-Host "X SPC entered fail routine (Port 0 = 0x$($spcFailed[-1].Value.ToString('X2')))" -ForegroundColor Red
        
        # 실패 값 확인 (Port 3)
        $failValue = $events | Where-Object { 
            $_.Source -eq "SPC700" -and $_.Action -eq "wrote" -and $_.Port -eq 3 
        } | Select-Object -Last 1
        
        if ($failValue) {
            Write-Host "  Fail value (Port 3): 0x$($failValue.Value.ToString('X2'))" -ForegroundColor Red
        }
    }
    
    Write-Host ""
}

# 타임라인 출력
if ($Timeline) {
    Write-Host "=== Timeline (last 20 events) ===" -ForegroundColor Yellow
    $events | Select-Object -Last 20 | ForEach-Object {
        $arrow = if ($_.Action -eq "wrote") { "->" } else { "<-" }
        Write-Host "$($_.Source) $arrow Port $($_.Port): 0x$($_.Value.ToString('X2')) (PC:0x$($_.PC.ToString('X4')))"
    }
}

Write-Host ""
Write-Host "Analysis complete." -ForegroundColor Cyan










