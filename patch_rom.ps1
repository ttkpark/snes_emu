$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")
$font = [System.IO.File]::ReadAllBytes("font.bin")

Write-Host "ROM size:" $rom.Length
Write-Host "Font size:" $font.Length

# Patch ROM at offset 0x4D2C
[Array]::Copy($font, 0, $rom, 0x4D2C, $font.Length)

# Save patched ROM
[System.IO.File]::WriteAllBytes("cputest-basic.sfc", $rom)

# Verify patch
Write-Host "Patched ROM at offset 0x4D2C"
Write-Host "First 32 bytes after patch:"
for ($i = 0; $i -lt 32; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x4D2C + $i])
    if (($i + 1) % 16 -eq 0) {
        Write-Host ""
    }
}
Write-Host ""


