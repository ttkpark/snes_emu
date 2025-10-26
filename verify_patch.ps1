$rom = [System.IO.File]::ReadAllBytes("cputest-basic.sfc")

# Character '0' is at offset 0x30 * 16 = 0x300 in font.bin
# In ROM, it should be at 0x4D2C + 0x300 = 0x502C
$char0_offset = 0x4D2C + (0x30 * 16)

Write-Host "Character '0' (0x30) data in ROM at offset 0x$($char0_offset.ToString('X')):"
for ($i = 0; $i -lt 16; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $rom[$char0_offset + $i])
}
Write-Host ""


