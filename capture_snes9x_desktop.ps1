#Requires -Version 5.0
# Desktop screen capture of Snes9x window (bypasses F12 hotkey issues).
# Uses GDI+ BitBlt to capture client area of Snes9x window.

$Snes9xExe = "C:\Users\GH\AppData\Local\Temp\snes9x\snes9x-x64.exe"
$RomPath   = "$PSScriptRoot\SNES Test Program.sfc"
$OutputDir = "$PSScriptRoot\tests\snes9x_ref"
$Snes9xDir = Split-Path $Snes9xExe -Parent

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
public class Cap {
    [DllImport("user32.dll")]
    public static extern IntPtr GetDC(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT pt);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int cmd);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr w, IntPtr l);

    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }

    public static void SaveWindow(IntPtr hWnd, string path) {
        RECT rc; GetClientRect(hWnd, out rc);
        int w = rc.R - rc.L, h = rc.B - rc.T;
        if (w <= 0 || h <= 0) return;
        POINT pt; pt.X = 0; pt.Y = 0; ClientToScreen(hWnd, ref pt);
        Bitmap bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        using (Graphics g = Graphics.FromImage(bmp)) {
            g.CopyFromScreen(pt.X, pt.Y, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
        }
        bmp.Save(path, ImageFormat.Png);
        bmp.Dispose();
    }
}
"@ -ReferencedAssemblies System.Drawing

function Get-Snes9xWindow {
    $p = Get-Process snes9x-x64 -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($p -and $p.MainWindowHandle -ne [IntPtr]::Zero) { return $p.MainWindowHandle }
    return [IntPtr]::Zero
}

function Focus($h) { [Cap]::ShowWindow($h, 9) | Out-Null; [Cap]::SetForegroundWindow($h) | Out-Null }

# Clean old output
Remove-Item "$OutputDir\*.png" -ErrorAction SilentlyContinue

# Launch Snes9x
Push-Location $Snes9xDir
$proc = Start-Process -PassThru -FilePath $Snes9xExe -ArgumentList "`"$RomPath`""
Pop-Location
Start-Sleep -Seconds 4

$h = Get-Snes9xWindow
if ($h -eq [IntPtr]::Zero) {
    Write-Error "Snes9x window not found"
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

Focus $h
Start-Sleep -Milliseconds 500

# Capture TC-01 main menu
Focus $h; Start-Sleep -Milliseconds 300
[Cap]::SaveWindow($h, "$OutputDir\tc01_menu.png")
Write-Host "Saved tc01_menu.png"

# Send Enter (START) to enter Electronics Test
# Using PostMessage WM_KEYDOWN 0x0D = VK_RETURN
Focus $h
[Cap]::PostMessage($h, 0x0100, [IntPtr]0x0D, [IntPtr]0x001C0001) | Out-Null
Start-Sleep -Milliseconds 100
[Cap]::PostMessage($h, 0x0101, [IntPtr]0x0D, [IntPtr]0xC01C0001) | Out-Null
Start-Sleep -Seconds 3

# Capture snapshots over time (Electronics Test cycles)
for ($i = 2; $i -le 12; $i++) {
    Start-Sleep -Seconds 3
    Focus $h; Start-Sleep -Milliseconds 200
    $name = "tc{0:D2}.png" -f $i
    [Cap]::SaveWindow($h, "$OutputDir\$name")
    Write-Host "Saved $name"
}

# Exit tests, navigate to Color Test
Start-Sleep -Seconds 2
Focus $h
# SELECT = VK_SHIFT 0x10
[Cap]::PostMessage($h, 0x0100, [IntPtr]0x10, [IntPtr]0x002A0001) | Out-Null
Start-Sleep -Milliseconds 100
[Cap]::PostMessage($h, 0x0101, [IntPtr]0x10, [IntPtr]0xC02A0001) | Out-Null
Start-Sleep -Seconds 2

# Final TC-13 capture
Focus $h
[Cap]::SaveWindow($h, "$OutputDir\tc13_color.png")
Write-Host "Saved tc13_color.png"

# Cleanup
Start-Sleep -Seconds 1
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# List results
Write-Host "---Reference screenshots---"
Get-ChildItem "$OutputDir\*.png" | ForEach-Object {
    $sz = [math]::Round($_.Length / 1024, 1)
    Write-Host "  $($_.Name) ($sz KB)"
}
