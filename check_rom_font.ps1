$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")
Write-Host "ROM size:" $rom.Length
Write-Host "Font data at 0x4D2C (first 32 bytes):"
for ($i = 0; $i -lt 32; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x4D2C + $i])
    if (($i + 1) % 16 -eq 0) {
        Write-Host ""
    }
}
Write-Host ""


