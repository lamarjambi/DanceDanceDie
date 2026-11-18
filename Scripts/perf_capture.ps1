# Standalone perf capture. NO git.
# usage: perf_capture.ps1 -Map /Game/UI/MainMenu -Label menu_base [-CVars "r.Foo=1,r.Bar=2"] [-Extra "-dx11"] [-Frames 2400] [-ShotAt 38]
# Runs the game for -Frames frames (exits by itself), grabs the screen at -ShotAt seconds -> Saved\perf_<label>.png,
# then perf_report.ps1 -> Saved\perf_<label>.txt. -CVars are test overrides for this run only (nothing is saved).
# Minimise the editor first: it shares the GPU.
param([string]$Map = '/Game/NightCityCreator/maps/NightCityCreator', [string]$Label = 'run', [string]$CVars = '',
      [string]$Extra = '', [int]$Frames = 2400, [int]$ShotAt = 38)
$root = Split-Path $PSScriptRoot -Parent
$exe = 'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe'
$a = "`"$root\DanceDanceDie.uproject`" $Map -game -windowed -ResX=1920 -ResY=1080 -csvGpuStats -csvCaptureFrames=$Frames -ExitAfterCsvProfiling -abslog=`"$root\Saved\Logs\perf_$Label.log`""
$cv = 't.MaxFPS=0'   # lift the 60 fps cap from DefaultEngine.ini so the real headroom shows
if ($CVars) { $cv += ",$CVars" }
$a += " -dpcvars=`"$cv`""
if ($Extra) { $a += " $Extra" }
$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
if ($ShotAt -gt 0) {
  Start-Sleep -Seconds $ShotAt
  if (-not $p.HasExited) {
    Add-Type -AssemblyName System.Drawing
    Add-Type -Namespace W -Name U -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);'
    [void][W.U]::SetProcessDPIAware()
    $w = [W.U]::GetSystemMetrics(0); $h = [W.U]::GetSystemMetrics(1)
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen(0, 0, 0, 0, $bmp.Size)
    $bmp.Save("$root\Saved\perf_$Label.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
  }
}
$p.WaitForExit()
& "$PSScriptRoot\perf_report.ps1" -Label $Label
