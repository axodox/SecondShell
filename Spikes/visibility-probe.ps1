#What does the child session see of files and registry values written from this side?
#Creates a folder in %LOCALAPPDATA%, an HKCU value and a RunOnce value, then a Startup folder script reports
#from inside the child session what it sees (into C:\Users\Public, which is known to work in both directions).
$ErrorActionPreference = "Stop"
$report = "C:\Users\Public\SecondShellVisibility.log"
$folder = Join-Path $env:LOCALAPPDATA "SecondShellVis"
$startupCmd = Join-Path ([Environment]::GetFolderPath("Startup")) "SecondShellVis.cmd"
$cv = "HKCU:\Software\Microsoft\Windows\CurrentVersion"
if (Test-Path $report) { [IO.File]::Delete($report) }

$check = "`$r = @(); `$r += 'session=' + (Get-Process -Id `$PID).SessionId; " +
  "`$r += 'SecondShellVis\host.txt: ' + (Test-Path '$folder\host.txt'); " +
  "`$r += 'SecondShell.Spikes: ' + (Test-Path (Join-Path `$env:LOCALAPPDATA 'SecondShell.Spikes')); " +
  "`$r += 'Local SecondShell*: ' + ((Get-ChildItem `$env:LOCALAPPDATA -Filter 'SecondShell*' -Force | ForEach-Object Name) -join ', '); " +
  "`$r += 'HKCU SecondShellVis: ' + (Get-ItemProperty HKCU:\Software\SecondShellVis -ErrorAction SilentlyContinue).Value; " +
  "`$r += 'RunOnce values: ' + ((Get-Item HKCU:\Software\Microsoft\Windows\CurrentVersion\RunOnce -ErrorAction SilentlyContinue).GetValueNames() -join ', '); " +
  "Set-Content -Path '$report' -Value `$r"

try
{
  New-Item $folder -ItemType Directory -Force | Out-Null
  Set-Content (Join-Path $folder "host.txt") "written by the user's session"
  New-Item HKCU:\Software\SecondShellVis -Force | New-ItemProperty -Name Value -Value "written by the user's session" -Force | Out-Null
  New-ItemProperty "$cv\RunOnce" -Name SecondShellVis -Value "cmd.exe /c exit" -PropertyType String -Force | Out-Null
  Set-Content $startupCmd ("@powershell.exe -NoProfile -WindowStyle Hidden -Command `"$check`"") -Encoding ascii

  $hostExe = Join-Path $PSScriptRoot "bin\x64\Release\RdpHost\RdpHost.exe"
  $process = Start-Process $hostExe -ArgumentList "--label", "visibility-probe", "--timeout", "40" -PassThru
  $process.WaitForExit(240000) | Out-Null
}
finally
{
  if (Test-Path $startupCmd) { [IO.File]::Delete($startupCmd) }
  Remove-ItemProperty "$cv\RunOnce" -Name SecondShellVis -ErrorAction SilentlyContinue
  reg.exe delete HKCU\Software\SecondShellVis /f 2>&1 | Out-Null
  if (Test-Path "$folder\host.txt") { [IO.File]::Delete("$folder\host.txt") }
  if (Test-Path $folder) { [IO.Directory]::Delete($folder) }
}

"--- seen from the child session:"
Get-Content $report -ErrorAction SilentlyContinue
"--- seen from here:"
"RunOnce values now: " + ((Get-Item "$cv\RunOnce" -ErrorAction SilentlyContinue).GetValueNames() -join ', ')
