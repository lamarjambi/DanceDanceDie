# Club party check in a Standalone game: fake win -> solid dancers, walk-into test, stare idles. NO git. Saves no assets.
# usage: party_test.ps1 [-Label x] -> Saved\party_test.txt + Saved\party_<label>_party.png / _stare.png. See Scripts/party_test.py.
param([string]$Label = 'run', [int]$TimeoutSec = 150)
$root = Split-Path $PSScriptRoot -Parent
$exe = 'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe'
$shots = "$root\Saved\Screenshots\WindowsEditor"
$before = @(); if (Test-Path $shots) { $before = @(Get-ChildItem $shots -Filter 'HighresScreenshot*.png' | ForEach-Object { $_.FullName }) }
$py = ($root -replace '\\', '/') + '/Scripts/party_test.py'
$a = "`"$root\DanceDanceDie.uproject`" /Game/CyberClub/DemoMap/DemoMap -game -windowed -ResX=1280 -ResY=720 -abslog=`"$root\Saved\Logs\party_$Label.log`" -dpcvars=`"t.MaxFPS=0`" -ExecCmds=`"DisableAllScreenMessages,py $py`""
$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); 'TIMEOUT: game killed' }
Get-Content "$root\Saved\party_test.txt"
$new = @(Get-ChildItem $shots -Filter 'HighresScreenshot*.png' | Where-Object { $before -notcontains $_.FullName } | Sort-Object Name)
$names = @(Select-String -Path "$root\Saved\party_test.txt" -Pattern '^shot (\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
for ($i = 0; $i -lt $new.Count; $i++) {
  $n = if ($i -lt $names.Count) { $names[$i] } else { "extra$i" }
  Move-Item $new[$i].FullName "$root\Saved\party_${Label}_$n.png" -Force
  "Saved\party_${Label}_$n.png"
}
