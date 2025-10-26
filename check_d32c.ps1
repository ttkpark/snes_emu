$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")

Write-Host "ROM 0xD32C (0xCD2C + 0x600, where char '0' is read by DMA):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0xD32C + $i])
}
Write-Host ""

Write-Host "`nROM 0x532C (0x4D2C + 0x600, where char '0' should be):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x532C + $i])
}
Write-Host ""

