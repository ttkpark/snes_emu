$font = [System.IO.File]::ReadAllBytes("font.bin")
Write-Host "Font.bin size:" $font.Length
Write-Host "First 32 bytes:"
for ($i = 0; $i -lt 32; $i++) {
    Write-Host -NoNewline ("{0:X2} " -f $font[$i])
    if (($i + 1) % 16 -eq 0) {
        Write-Host ""
    }
}
Write-Host ""


