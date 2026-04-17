#Requires -Version 5.0
# Automates Snes9x with Win32 window focus to capture reference screenshots.

$Snes9xExe = "C:\Users\GH\AppData\Local\Temp\snes9x\snes9x-x64.exe"
$RomPath   = "$PSScriptRoot\SNES Test Program.sfc"
$OutputDir = "$PSScriptRoot\tests\snes9x_ref"
$Snes9xDir = Split-Path $Snes9xExe -Parent

if (-not (Test-Path $Snes9xExe)) { Write-Error "Snes9x missing"; exit 1 }
if (-not (Test-Path $RomPath))   { Write-Error "ROM missing";    exit 1 }

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Win32 API for focus + key injection
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int cmd);

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT {
        public uint type;
        public InputUnion u;
    }
    [StructLayout(LayoutKind.Explicit)]
    public struct InputUnion {
        [FieldOffset(0)] public KEYBDINPUT ki;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT {
        public ushort wVk;
        public ushort wScan;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }
    [DllImport("user32.dll")]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern int MapVirtualKey(uint uCode, uint uMapType);

    public const uint WM_KEYDOWN = 0x0100;
    public const uint WM_KEYUP   = 0x0101;

    public static void PressKey(ushort vk) {
        INPUT[] inputs = new INPUT[2];
        inputs[0].type = 1; // INPUT_KEYBOARD
        inputs[0].u.ki.wVk = vk;
        inputs[0].u.ki.dwFlags = 0; // key down
        inputs[1].type = 1;
        inputs[1].u.ki.wVk = vk;
        inputs[1].u.ki.dwFlags = 2; // key up
        SendInput(2, inputs, Marshal.SizeOf(typeof(INPUT)));
    }

    public static void PostKey(IntPtr hWnd, ushort vk) {
        int scan = MapVirtualKey((uint)vk, 0);
        IntPtr lParamDown = (IntPtr)((scan << 16) | 1);
        IntPtr lParamUp   = (IntPtr)(unchecked((int)0xC0000001) | (scan << 16));
        PostMessage(hWnd, WM_KEYDOWN, (IntPtr)vk, lParamDown);
        System.Threading.Thread.Sleep(50);
        PostMessage(hWnd, WM_KEYUP,   (IntPtr)vk, lParamUp);
    }
}
"@

function Focus-Snes9x {
    # Use process main window handle directly (more reliable)
    $procs = Get-Process snes9x-x64 -ErrorAction SilentlyContinue
    foreach ($p in $procs) {
        if ($p.MainWindowHandle -ne [IntPtr]::Zero) {
            [Win32]::ShowWindow($p.MainWindowHandle, 9) | Out-Null  # SW_RESTORE
            [Win32]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
            return $true
        }
    }
    return $false
}

function Send-VK($vk, $pauseMs = 200) {
    # Try PostMessage to Snes9x window first (works without focus)
    $procs = Get-Process snes9x-x64 -ErrorAction SilentlyContinue
    $sent = $false
    foreach ($p in $procs) {
        if ($p.MainWindowHandle -ne [IntPtr]::Zero) {
            [Win32]::PostKey($p.MainWindowHandle, [uint16]$vk)
            $sent = $true
            break
        }
    }
    if (-not $sent) {
        [Win32]::PressKey([uint16]$vk)
    }
    Start-Sleep -Milliseconds $pauseMs
}

# Virtual keys
$VK_F12   = 0x7B
$VK_ENTER = 0x0D
$VK_SHIFT = 0x10
$VK_X     = 0x58  # SNES A
$VK_Z     = 0x5A  # SNES B

# Ensure Screenshots directory configured correctly
$shotsDir = Join-Path $Snes9xDir "Screenshots"
New-Item -ItemType Directory -Force -Path $shotsDir | Out-Null
# Clear previous shots
Remove-Item "$shotsDir\*.png" -ErrorAction SilentlyContinue

Push-Location $Snes9xDir
$proc = Start-Process -PassThru -FilePath $Snes9xExe -ArgumentList "`"$RomPath`""
Start-Sleep -Seconds 4
Pop-Location

# Focus window
$focused = Focus-Snes9x
if (-not $focused) { Write-Warning "Focus failed, trying anyway" }
Start-Sleep -Milliseconds 500

# Capture sequence with explicit focus before each keypress
function Capture($label, $waitSec) {
    Start-Sleep -Seconds $waitSec
    Focus-Snes9x | Out-Null
    Start-Sleep -Milliseconds 300
    Send-VK $VK_F12 400
    Write-Host "[$label] F12 sent"
}

Capture "TC-01 menu" 2

# Enter Electronics Test (item 1 is default)
Focus-Snes9x | Out-Null; Send-VK $VK_ENTER 500

# Capture through TC-02 ~ TC-12 (Electronics Test cycles sub-tests each ~3s)
for ($i = 2; $i -le 12; $i++) {
    Capture "TC-$($i.ToString('00'))" 3
}

# Exit back to menu
Focus-Snes9x | Out-Null; Send-VK $VK_SHIFT 500  # SELECT
Start-Sleep -Seconds 2

# Navigate to Color Test (item 5) via SELECT 4x
for ($i = 0; $i -lt 4; $i++) {
    Focus-Snes9x | Out-Null; Send-VK $VK_SHIFT 400
}
Focus-Snes9x | Out-Null; Send-VK $VK_ENTER 500
Capture "TC-13 Color Test" 3

# Cleanup
Start-Sleep -Seconds 1
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# Move screenshots
$found = Get-ChildItem "$shotsDir\*.png" -ErrorAction SilentlyContinue
Write-Host "Found $($found.Count) screenshots in $shotsDir"
if ($found.Count -gt 0) {
    Copy-Item "$shotsDir\*.png" $OutputDir -Force
    Get-ChildItem $OutputDir | Select-Object Name | Format-Table
} else {
    Write-Warning "No screenshots captured. F12 may not be reaching Snes9x."
    Write-Host "Run snes9x manually and press F12 during each test screen to populate tests/snes9x_ref/"
}
