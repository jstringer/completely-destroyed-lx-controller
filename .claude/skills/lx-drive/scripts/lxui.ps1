<#
  lxui.ps1 -- drive and screenshot the running lxcontrol UI.

    lxui.ps1 state                        # dump mode / tab / program / voices / clickable labels
    lxui.ps1 tab PROGRAMS
    lxui.ps1 click "+ New Patch"
    lxui.ps1 click "Fire#2"               # 2nd "Fire" drawn this frame (see CLICKABLE in the ack)
    lxui.ps1 mode perform
    lxui.ps1 text patch Wash
    lxui.ps1 resize "1600 900"
    lxui.ps1 shot -Out C:\path\shot.png   # PNG of the app window

  Control goes through .agent/{cmd,ack} which the app polls once per frame (src/lxagent.h).
  Pixels are captured with Win32 window capture: nap::Snapshot cannot see ImGui.
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, Mandatory = $true)][string]$Verb,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$Rest,
    [string]$Out,
    [string]$Dir,
    [string]$Process = 'lxcontrol',
    [int]$TimeoutMs = 4000
)

$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
if (-not $Dir) { $Dir = if ($env:LXUI_DIR) { $env:LXUI_DIR } else { Join-Path $appRoot '.agent' } }
$arg = if ($Rest) { $Rest -join ' ' } else { '' }

function Get-AppWindow {
    $p = Get-Process -Name $Process -ErrorAction SilentlyContinue |
         Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if (-not $p) { throw "$Process is not running (or has no window). Start bin/Release-x86_64/$Process.exe first." }
    return $p.MainWindowHandle
}

function Send-Command([string]$line) {
    if (-not (Test-Path $Dir)) { New-Item -ItemType Directory -Path $Dir -Force | Out-Null }
    $ack = Join-Path $Dir 'ack'
    Remove-Item $ack -ErrorAction SilentlyContinue
    # Write to a temp file and rename into place: the app polls once per frame and would otherwise
    # happily read (and delete) a created-but-not-yet-written cmd file. UTF8 without BOM, since the
    # app reads this with std::getline and matches labels byte-for-byte (■ All Stop et al).
    $tmp = Join-Path $Dir 'cmd.tmp'
    [IO.File]::WriteAllText($tmp, "$line`n", (New-Object Text.UTF8Encoding $false))
    Move-Item -LiteralPath $tmp -Destination (Join-Path $Dir 'cmd') -Force

    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs) {
        if (Test-Path $ack) {
            Start-Sleep -Milliseconds 20   # let the app finish writing
            [IO.File]::ReadAllText($ack, [Text.Encoding]::UTF8)
            return
        }
        Start-Sleep -Milliseconds 25
    }
    throw "no ack after ${TimeoutMs}ms -- is $Process running, focused-or-not, and drawing frames? (cmd dir: $Dir)"
}

function Save-Shot([string]$path) {
    Add-Type -AssemblyName System.Drawing
    if (-not ([Management.Automation.PSTypeName]'LxWin').Type) {
        Add-Type -Namespace '' -Name LxWin -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
public struct RECT { public int Left, Top, Right, Bottom; }
'@
    }

    $h = Get-AppWindow
    $r = New-Object 'LxWin+RECT'
    [void][LxWin]::GetWindowRect($h, [ref]$r)
    $w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
    if ($w -le 0 -or $ht -le 0) { throw 'window has no size (minimized?)' }

    $bmp = New-Object Drawing.Bitmap $w, $ht
    $g = [Drawing.Graphics]::FromImage($bmp)

    # PW_RENDERFULLCONTENT (0x2) works for many accelerated windows; Vulkan swapchains often come
    # back black, so verify and fall back to grabbing the screen with the window raised.
    $hdc = $g.GetHdc()
    [void][LxWin]::PrintWindow($h, $hdc, 2)
    $g.ReleaseHdc($hdc)

    $bright = 0
    for ($y = 0; $y -lt $ht; $y += 17) {
        for ($x = 0; $x -lt $w; $x += 17) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.R + $c.G + $c.B -gt 60) { $bright++ }
        }
    }
    if ($bright -lt 20) {
        [void][LxWin]::ShowWindow($h, 9)          # SW_RESTORE
        [void][LxWin]::SetForegroundWindow($h)
        Start-Sleep -Milliseconds 250
        [void][LxWin]::GetWindowRect($h, [ref]$r)
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object Drawing.Size $w, $ht))
    }

    $dir = Split-Path -Parent $path
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $bmp.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output $path
}

switch ($Verb) {
    'shot' {
        if (-not $Out) { $Out = Join-Path $env:TEMP ('lxui-{0}.png' -f (Get-Date -Format 'HHmmss')) }
        Save-Shot $Out
    }
    default {
        # Everything else is a bridge verb: click / tab / mode / text / styleguide / resize / stopall / state
        $line = if ($arg) { "$Verb $arg" } else { $Verb }
        Send-Command $line
    }
}
