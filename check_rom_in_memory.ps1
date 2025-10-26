# Read ROM file
$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")

Write-Host "ROM file size:" $rom.Length

# Check if font data exists at 0x4D2C
Write-Host "`nFont data at ROM offset 0x4D2C (first 32 bytes):"
for ($i = 0; $i -lt 32; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x4D2C + $i])
    if (($i + 1) % 16 -eq 0) {
        Write-Host ""
    }
}

# Check character '0' at 0x502C
Write-Host "`nCharacter '0' (0x30) data at ROM offset 0x502C (first 16 bytes):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x502C + $i])
}
Write-Host ""

