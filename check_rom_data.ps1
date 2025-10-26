$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")

Write-Host "Checking ROM data at different offsets:"
Write-Host ""

Write-Host "0x4D2C (first 16 bytes):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x4D2C + $i])
}
Write-Host ""

Write-Host "`n0x502C (char '0', first 16 bytes):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x502C + $i])
}
Write-Host ""

Write-Host "`n0x4F2C (offset +0x300, first 16 bytes):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[0x4F2C + $i])
}
Write-Host ""

