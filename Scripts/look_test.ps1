# Look-down check of the player's own body in a Standalone game. NO git. Saves no assets.
# usage: look_test.ps1 -Label base [-Steps "label:pitch:fwd:up:nearclip;..."] [-At "x,y,yaw"] [-Walk] [-Map /Game/...]
# -> Saved\look_<label>_<step>.png + Saved\look_info.txt. See Scripts/look_test.py for the step format.
param([string]$Label = 'base', [string]$Steps = '', [string]$At = '', [switch]$Walk, [string]$Map = '/Game/NightCityCreator/maps/NightCityCreator', [int]$TimeoutSec = 150)
$root = Split-Path $PSScriptRoot -Parent
$exe = 'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe'
$shots = "$root\Saved\Screenshots\WindowsEditor"
$before = @(); if (Test-Path $shots) { $before = @(Get-ChildItem $shots -Filter 'HighresScreenshot*.png' | ForEach-Object { $_.FullName }) }
if ($Steps) { $env:DDD_LOOK_STEPS = $Steps } else { Remove-Item Env:DDD_LOOK_STEPS -ErrorAction SilentlyContinue }
if ($At) { $env:DDD_LOOK_AT = $At } else { Remove-Item Env:DDD_LOOK_AT -ErrorAction SilentlyContinue }
if ($Walk) { $env:DDD_LOOK_WALK = '1' } else { Remove-Item Env:DDD_LOOK_WALK -ErrorAction SilentlyContinue }
$py = ($root -replace '\\', '/') + '/Scripts/look_test.py'
$a = "`"$root\DanceDanceDie.uproject`" $Map -game -windowed -ResX=1280 -ResY=720 -abslog=`"$root\Saved\Logs\look_$Label.log`" -ExecCmds=`"DisableAllScreenMessages,py $py`""
$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); 'TIMEOUT: game killed' }
Get-Content "$root\Saved\look_info.txt"
$new = @(Get-ChildItem $shots -Filter 'HighresScreenshot*.png' | Where-Object { $before -notcontains $_.FullName } | Sort-Object Name)
$names = @(Select-String -Path "$root\Saved\look_info.txt" -Pattern '^shot (\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
for ($i = 0; $i -lt $new.Count; $i++) {
  $n = if ($i -lt $names.Count) { $names[$i] } else { "extra$i" }
  Move-Item $new[$i].FullName "$root\Saved\look_${Label}_$n.png" -Force
  "Saved\look_${Label}_$n.png"
}
