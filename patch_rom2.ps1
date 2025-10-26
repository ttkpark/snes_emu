$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")
$font = [System.IO.File]::ReadAllBytes("font.bin")

Write-Host "ROM size:" $rom.Length
Write-Host "Font size:" $font.Length
Write-Host "Font first 16 bytes:" ([System.BitConverter]::ToString($font[0..15]))

# Patch ROM at offset 0x4D2C using loop
for ($i = 0; $i -lt $font.Length; $i++) {
    $rom[0x4D2C + $i] = $font[$i]
}

# Save patched ROM
[System.IO.File]::WriteAllBytes("cputest-basic.sfc", $rom)

# Verify patch by re-reading
$rom_verify = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")
Write-Host "Patched ROM at offset 0x4D2C"
Write-Host "First 32 bytes after patch:"
for ($i = 0; $i -lt 32; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom_verify[0x4D2C + $i])
    if (($i + 1) % 16 -eq 0) {
        Write-Host ""
    }
}
Write-Host ""


