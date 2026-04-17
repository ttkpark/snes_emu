#Requires -Version 5.0
# Automates Snes9x to capture reference screenshots for each test card.
# Launches Snes9x with "SNES Test Program.sfc", sends F12 (screenshot) + navigation keys.
# Output: .\tests\snes9x_ref\frame_*.png

$Snes9xExe = "C:\Users\GH\AppData\Local\Temp\snes9x\snes9x-x64.exe"
$RomPath   = "$PSScriptRoot\SNES Test Program.sfc"
$OutputDir = "$PSScriptRoot\tests\snes9x_ref"
$Snes9xDir = Split-Path $Snes9xExe -Parent

# Path exists?
if (-not (Test-Path $Snes9xExe)) {
    Write-Error "Snes9x not found at $Snes9xExe. Install first."
    exit 1
}
if (-not (Test-Path $RomPath)) {
    Write-Error "ROM not found at $RomPath"
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type -AssemblyName System.Windows.Forms

# Launch Snes9x with ROM
Push-Location $Snes9xDir
$proc = Start-Process -PassThru -FilePath $Snes9xExe -ArgumentList "`"$RomPath`""
Start-Sleep -Milliseconds 3000  # let window open
Pop-Location

# Helper: send a key
function Send-Key($key, $holdMs = 100) {
    [System.Windows.Forms.SendKeys]::SendWait($key)
    Start-Sleep -Milliseconds $holdMs
}

# Sequence: wait for main menu, capture TC-01, then navigate through tests.
# SNES pad mapping in Snes9x default: A = X key, B = Z key, Start = Enter, Select = Shift
# F12 = screenshot

Start-Sleep -Seconds 2  # wait for ROM to reach main menu

Write-Host "Capturing TC-01 Main Menu"
Send-Key "{F12}"

# Start Electronics Test (item 1)
Send-Key "{ENTER}"  # Start button
Start-Sleep -Seconds 1

# Electronics Test cycles through TC-02 through TC-12 automatically
# Capture at intervals
for ($i = 2; $i -le 12; $i++) {
    Start-Sleep -Seconds 3
    Write-Host "Capturing TC-$($i.ToString('00'))"
    Send-Key "{F12}"
}

# Press SELECT (+Shift) to exit test, then navigate to next test
Start-Sleep -Seconds 2
Send-Key "+"  # SELECT
Start-Sleep -Seconds 1

# Navigate to Color Test (item 5) via SELECT cycling
for ($i = 0; $i -lt 4; $i++) {
    Send-Key "+"
    Start-Sleep -Milliseconds 500
}
Send-Key "{ENTER}"  # START Color Test
Start-Sleep -Seconds 3
Write-Host "Capturing TC-13 Color Test"
Send-Key "{F12}"

# Done. Close Snes9x.
Start-Sleep -Seconds 1
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# Copy screenshots
$shots = Join-Path $Snes9xDir "Screenshots"
if (Test-Path $shots) {
    Copy-Item "$shots\*.png" $OutputDir -Force
    Write-Host "Reference screenshots saved to: $OutputDir"
    Get-ChildItem $OutputDir | ForEach-Object { Write-Host "  $($_.Name)" }
} else {
    Write-Warning "No screenshots found at $shots"
}
