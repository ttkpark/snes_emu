# Analyze infinite loops in APU trace log
param(
    [string]$TraceFile = "apu_trace.log",
    [int]$ThresholdCount = 100,
    [int]$TailLines = 10000
)

Write-Host "=== Infinite Loop Detector ===" -ForegroundColor Green
Write-Host "Trace file: $TraceFile" -ForegroundColor Cyan
Write-Host "Threshold: $ThresholdCount repetitions" -ForegroundColor Cyan

if (-not (Test-Path $TraceFile)) {
    Write-Host "ERROR: Trace file not found: $TraceFile" -ForegroundColor Red
    exit 1
}

Write-Host "`nAnalyzing last $TailLines lines..." -ForegroundColor Yellow

$lines = Get-Content $TraceFile -Tail $TailLines

$pcCounts = @{}
$pcStates = @{}

foreach ($line in $lines) {
    if ($line -match "PC:0x([0-9a-fA-F]+).*A:0x([0-9a-fA-F]+).*X:0x([0-9a-fA-F]+).*Y:0x([0-9a-fA-F]+).*PSW:0x([0-9a-fA-F]+)") {
        $pc = $matches[1]
        $a = $matches[2]
        $x = $matches[3]
        $y = $matches[4]
        $psw = $matches[5]
        
        if (-not $pcCounts.ContainsKey($pc)) {
            $pcCounts[$pc] = 0
            $pcStates[$pc] = @()
        }
        $pcCounts[$pc]++
        
        $state = "A:$a X:$x Y:$y PSW:$psw"
        $pcStates[$pc] += $state
    }
}

$topPCs = $pcCounts.GetEnumerator() | Sort-Object -Property Value -Descending | Select-Object -First 10

Write-Host "`nTop 10 Most Executed PC Addresses:" -ForegroundColor Cyan
$loopDetected = $false

foreach ($entry in $topPCs) {
    $pc = $entry.Key
    $count = $entry.Value
    
    $color = if ($count -gt $ThresholdCount) { "Red" } else { "Gray" }
    $marker = if ($count -gt $ThresholdCount) { "[LOOP]" } else { "      " }
    
    Write-Host "$marker PC: 0x$pc - Executed $count times" -ForegroundColor $color
    
    if ($count -gt $ThresholdCount) {
        $loopDetected = $true
        
        $states = $pcStates[$pc]
        $uniqueStates = $states | Select-Object -Unique
        
        Write-Host "   Unique states: $($uniqueStates.Count)" -ForegroundColor Yellow
        
        if ($uniqueStates.Count -eq 1) {
            Write-Host "   WARNING: Always in same state: $($uniqueStates[0])" -ForegroundColor Red
            Write-Host "   This is likely an infinite loop!" -ForegroundColor Red
        } elseif ($uniqueStates.Count -le 5) {
            Write-Host "   States:" -ForegroundColor Yellow
            $uniqueStates | Select-Object -First 5 | ForEach-Object {
                Write-Host "      $_" -ForegroundColor Gray
            }
        }
        
        $instruction = $lines | Where-Object { $_ -match "PC:0x$pc" } | Select-Object -First 1
        if ($instruction -match "\| ([0-9a-fA-F ]+) \| ([A-Z]+)") {
            $opcode = $matches[1].Trim()
            $mnemonic = $matches[2].Trim()
            Write-Host "   Instruction: $opcode - $mnemonic" -ForegroundColor Yellow
        }
        
        Write-Host ""
    }
}

if ($loopDetected) {
    Write-Host "`nLoop Pattern Analysis:" -ForegroundColor Cyan
    
    $recentPCs = @()
    $lines | Select-Object -Last 100 | ForEach-Object {
        if ($_ -match "PC:0x([0-9a-fA-F]+)") {
            $recentPCs += $matches[1]
        }
    }
    
    if ($recentPCs.Count -ge 10) {
        Write-Host "   Last 20 PC addresses:" -ForegroundColor Yellow
        $recentPCs | Select-Object -Last 20 | ForEach-Object {
            Write-Host "      0x$_" -ForegroundColor Gray
        }
    }
    
    Write-Host "`nRecommendations:" -ForegroundColor Cyan
    Write-Host "   1. Check the instruction at the looping PC address" -ForegroundColor Yellow
    Write-Host "   2. Verify flag calculations (especially for branch instructions)" -ForegroundColor Yellow
    Write-Host "   3. Check if the expected state change is happening" -ForegroundColor Yellow
    Write-Host "   4. Review port communication logs for sync issues" -ForegroundColor Yellow
    
} else {
    Write-Host "`nNo infinite loops detected" -ForegroundColor Green
    Write-Host "   All PC addresses executed less than $ThresholdCount times" -ForegroundColor Gray
}

Write-Host "`nLast Execution State:" -ForegroundColor Cyan
$lastLine = $lines[-1]
if ($lastLine -match "PC:0x([0-9a-fA-F]+).*A:0x([0-9a-fA-F]+).*X:0x([0-9a-fA-F]+).*Y:0x([0-9a-fA-F]+).*SP:0x([0-9a-fA-F]+).*PSW:0x([0-9a-fA-F]+)") {
    Write-Host "   PC:  0x$($matches[1])" -ForegroundColor Gray
    Write-Host "   A:   0x$($matches[2])" -ForegroundColor Gray
    Write-Host "   X:   0x$($matches[3])" -ForegroundColor Gray
    Write-Host "   Y:   0x$($matches[4])" -ForegroundColor Gray
    Write-Host "   SP:  0x$($matches[5])" -ForegroundColor Gray
    Write-Host "   PSW: 0x$($matches[6])" -ForegroundColor Gray
}










