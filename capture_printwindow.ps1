#Requires -Version 5.0
# PrintWindow API to capture Snes9x window even if not foreground.

$Snes9xExe = "C:\Users\GH\AppData\Local\Temp\snes9x\snes9x-x64.exe"
$RomPath   = "$PSScriptRoot\SNES Test Program.sfc"
$OutputDir = "$PSScriptRoot\tests\snes9x_ref"
$Snes9xDir = Split-Path $Snes9xExe -Parent

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Remove-Item "$OutputDir\*.png" -ErrorAction SilentlyContinue

Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
public class PWCap {
    [DllImport("user32.dll")] public static extern IntPtr GetDesktopWindow();
    [DllImport("user32.dll")] public static extern IntPtr GetWindowDC(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);

    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }

    public static bool Save(IntPtr hWnd, string path) {
        RECT rc;
        if (!GetWindowRect(hWnd, out rc)) return false;
        int w = rc.R - rc.L, h = rc.B - rc.T;
        if (w <= 0 || h <= 0) return false;
        Bitmap bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        using (Graphics g = Graphics.FromImage(bmp)) {
            IntPtr hdc = g.GetHdc();
            bool ok = PrintWindow(hWnd, hdc, 2); // PW_RENDERFULLCONTENT
            g.ReleaseHdc(hdc);
            if (!ok) return false;
        }
        bmp.Save(path, ImageFormat.Png);
        bmp.Dispose();
        return true;
    }
}
"@

Push-Location $Snes9xDir
$proc = Start-Process -PassThru -FilePath $Snes9xExe -ArgumentList "`"$RomPath`""
Pop-Location
Start-Sleep -Seconds 5

$p = Get-Process snes9x-x64 -ErrorAction SilentlyContinue | Select-Object -First 1
$h = $p.MainWindowHandle
Write-Host "Snes9x hWnd: $h"

# Capture TC-01 menu
$ok = [PWCap]::Save($h, "$OutputDir\tc01_menu.png")
Write-Host "TC-01 menu captured: $ok"

# Let the ROM auto-cycle through Electronics Test (no input needed, it auto-demos?)
# Or press Enter via keybd_event to start
Add-Type -Namespace Win32 -Name API -MemberDefinition @'
[DllImport("user32.dll")]
public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
'@
[Win32.API]::keybd_event(0x0D, 0x1C, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 100
[Win32.API]::keybd_event(0x0D, 0x1C, 2, [UIntPtr]::Zero)
Start-Sleep -Seconds 3

# Capture over time
for ($i = 2; $i -le 12; $i++) {
    Start-Sleep -Seconds 3
    $name = "tc{0:D2}.png" -f $i
    $ok = [PWCap]::Save($h, "$OutputDir\$name")
    Write-Host "TC-$i captured: $ok"
}

# Final capture
Start-Sleep -Seconds 3
[PWCap]::Save($h, "$OutputDir\tc_final.png") | Out-Null
Write-Host "Final capture done"

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# List
Get-ChildItem "$OutputDir\*.png" | ForEach-Object {
    $sz = [math]::Round($_.Length / 1024, 1)
    Write-Host "  $($_.Name) ($sz KB)"
}
