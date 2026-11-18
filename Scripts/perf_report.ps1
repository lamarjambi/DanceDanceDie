# Summarises a CSV profiler capture (Saved\Profiling\CSV) -> Saved\perf_<label>.txt
# usage: perf_report.ps1 -Label city_base [-Last 600] [-Csv path]   (default: newest csv, last 600 frames = loading skipped)
param([string]$Label = 'run', [int]$Last = 600, [string]$Csv = '')
$root = Split-Path $PSScriptRoot -Parent
$dir = Join-Path $root 'Saved\Profiling\CSV'
if (-not $Csv) {
  $f = Get-ChildItem $dir -Filter *.csv -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if (-not $f) { Write-Output "no csv in $dir"; exit 1 }
  $Csv = $f.FullName
}
$inv = [Globalization.CultureInfo]::InvariantCulture
$num = [Globalization.NumberStyles]::Float
$lines = [IO.File]::ReadAllLines($Csv)
$hdr = $lines[0].Split(',')
$fi = [Array]::IndexOf($hdr, 'FrameTime')
if ($fi -lt 0) { Write-Output "no FrameTime column in $Csv"; exit 1 }
$rows = New-Object 'System.Collections.Generic.List[string[]]'
for ($i = 1; $i -lt $lines.Length; $i++) {
  $p = $lines[$i].Split(',')
  $v = 0.0
  if ($p.Length -gt $fi -and [double]::TryParse($p[$fi], $num, $inv, [ref]$v)) { $rows.Add($p) }
}
$total = $rows.Count
$start = [Math]::Max(0, $total - $Last)
$n = $total - $start
$mean = @{}
for ($c = 0; $c -lt $hdr.Length; $c++) {
  $s = 0.0; $k = 0
  for ($r = $start; $r -lt $total; $r++) {
    $p = $rows[$r]; $v = 0.0
    if ($c -lt $p.Length -and [double]::TryParse($p[$c], $num, $inv, [ref]$v)) { $s += $v; $k++ }
  }
  if ($k -gt 0) { $mean[$hdr[$c]] = $s / $k }
}
$ft = New-Object 'System.Collections.Generic.List[double]'
for ($r = $start; $r -lt $total; $r++) { $ft.Add([double]::Parse($rows[$r][$fi], $num, $inv)) }
$sorted = $ft.ToArray(); [Array]::Sort($sorted)
$p99 = $sorted[[Math]::Min($sorted.Length - 1, [int][Math]::Floor($sorted.Length * 0.99))]
$out = New-Object 'System.Collections.Generic.List[string]'
$out.Add("label: $Label")
$out.Add("csv:   $Csv")
$out.Add("frames: $total captured, last $n used")
$out.Add(('avg fps: {0:N1}   (frame {1:N2} ms, 1% worst frame {2:N2} ms = {3:N1} fps)' -f (1000.0 / $mean['FrameTime']), $mean['FrameTime'], $p99, (1000.0 / $p99)))
foreach ($k in 'GameThreadTime', 'RenderThreadTime', 'RHIThreadTime', 'GPUTime') {
  if ($mean.ContainsKey($k)) { $out.Add(('{0,-18} {1,7:N2} ms' -f $k, $mean[$k])) }
}
$out.Add('---- GPU passes (ms, biggest first)')
$mean.GetEnumerator() | Where-Object { $_.Key -like 'GPU/*' -and $_.Value -ge 0.05 } | Sort-Object Value -Descending |
  ForEach-Object { $out.Add(('{0,-44} {1,7:N2}' -f $_.Key, $_.Value)) }
$out.Add('---- draw calls / primitives')
$mean.GetEnumerator() | Where-Object { $_.Key -match '^(RHI|DrawCall|View|LightCount|Shadow)/' } | Sort-Object Key |
  ForEach-Object { $out.Add(('{0,-44} {1,10:N1}' -f $_.Key, $_.Value)) }
$dst = Join-Path $root "Saved\perf_$Label.txt"
[IO.File]::WriteAllLines($dst, $out)
$out | Select-Object -First 30 | Write-Output
Write-Output "-> $dst"
