#Which per-user startup mechanisms start a program in a new child session? Each one writes a tag line
#to the profile (child.log), to C:\Users\Public, to the repository and to HKCU\Software\SecondShellProbe.
#Arms all of them, signs in through RdpHost (no program of its own), then removes every entry again.
#-Approve also marks the Run and Startup folder entries as enabled in StartupApproved, like the user's own enabled entries.
param([switch]$Approve)
$ErrorActionPreference = "Stop"
$out = Join-Path $env:LOCALAPPDATA "SecondShell.Spikes"
New-Item $out -ItemType Directory -Force | Out-Null
Get-ChildItem $out -File | Remove-Item -Force
$log = Join-Path $out "child.log"
$public = "C:\Users\Public\SecondShellProbe.log"
$repo = Join-Path $PSScriptRoot "results\startup-probe.log"
New-Item (Split-Path $repo) -ItemType Directory -Force | Out-Null
if (Test-Path $repo) { [IO.File]::Delete($repo) }

function Command($tag)
{
  $script = "`$l = '$tag session=' + (Get-Process -Id `$PID).SessionId + ' at ' + (Get-Date -Format HH:mm:ss.fff) + ' localappdata=' + `$env:LOCALAPPDATA; " +
    "Add-Content -Path '$public' -Value `$l; Add-Content -Path '$repo' -Value `$l; " +
    "foreach (`$t in '$log', (Join-Path `$env:LOCALAPPDATA 'SecondShellProbe.log'), (Join-Path `$env:TEMP 'SecondShellProbe.log')) { try { Add-Content -Path `$t -Value `$l -ErrorAction Stop; Add-Content -Path '$public' -Value ('  wrote ' + `$t) } catch { Add-Content -Path '$public' -Value ('  FAILED ' + `$t + ': ' + `$_.Exception.GetType().Name + ' ' + `$_.Exception.Message) } }; " +
    "New-Item HKCU:\Software\SecondShellProbe -Force | New-ItemProperty -Name '$tag' -Value `$l -Force"
  "powershell.exe -NoProfile -WindowStyle Hidden -Command `"$script`""
}

$cv = "HKCU:\Software\Microsoft\Windows\CurrentVersion"
$policyRun = "$cv\Policies\Explorer\Run"
$approved = "$cv\Explorer\StartupApproved"
$startupCmd = Join-Path ([Environment]::GetFolderPath("Startup")) "SecondShellProbe.cmd"
$enabled = [byte[]](2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

try
{
  New-ItemProperty "$cv\RunOnce" -Name SecondShellProbe -Value (Command "RunOnce") -PropertyType String -Force | Out-Null
  New-ItemProperty "$cv\Run" -Name SecondShellProbe -Value (Command "Run") -PropertyType String -Force | Out-Null
  New-Item $policyRun -Force | Out-Null
  New-ItemProperty $policyRun -Name SecondShellProbe -Value (Command "PoliciesRun") -PropertyType String -Force | Out-Null
  Set-Content $startupCmd ("@" + (Command "StartupFolder")) -Encoding ascii
  New-ItemProperty "HKCU:\Environment" -Name UserInitMprLogonScript -Value (Command "LogonScript") -PropertyType String -Force | Out-Null
  if ($Approve)
  {
    New-ItemProperty "$approved\Run" -Name SecondShellProbe -Value $enabled -PropertyType Binary -Force | Out-Null
    New-ItemProperty "$approved\StartupFolder" -Name SecondShellProbe.cmd -Value $enabled -PropertyType Binary -Force | Out-Null
  }

  $hostExe = Join-Path $PSScriptRoot "bin\x64\Release\RdpHost\RdpHost.exe"
  $process = Start-Process $hostExe -ArgumentList "--label", "startup-probe", "--timeout", "60" -PassThru
  $process.WaitForExit(240000) | Out-Null
}
finally
{
  Remove-ItemProperty "$cv\RunOnce" -Name SecondShellProbe -ErrorAction SilentlyContinue
  Remove-ItemProperty "$cv\Run" -Name SecondShellProbe -ErrorAction SilentlyContinue
  Remove-ItemProperty $policyRun -Name SecondShellProbe -ErrorAction SilentlyContinue
  Remove-ItemProperty "HKCU:\Environment" -Name UserInitMprLogonScript -ErrorAction SilentlyContinue
  Remove-ItemProperty "$approved\Run" -Name SecondShellProbe -ErrorAction SilentlyContinue
  Remove-ItemProperty "$approved\StartupFolder" -Name SecondShellProbe.cmd -ErrorAction SilentlyContinue
  if (Test-Path $startupCmd) { [IO.File]::Delete($startupCmd) }
  reg.exe delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer\Run" /f 2>&1 | Out-Null
}

Get-Content (Join-Path $out "host.log") | Select-String "SIGNED|Child session|No child|WARNING"
"--- profile:"; Get-Content $log -ErrorAction SilentlyContinue; Get-Content (Join-Path $env:LOCALAPPDATA "SecondShellProbe.log") -ErrorAction SilentlyContinue; Get-Content (Join-Path $env:TEMP "SecondShellProbe.log") -ErrorAction SilentlyContinue
foreach ($f in (Join-Path $env:LOCALAPPDATA "SecondShellProbe.log"), (Join-Path $env:TEMP "SecondShellProbe.log")) { if (Test-Path $f) { [IO.File]::Delete($f) } }
"--- public:"; Get-Content $public -ErrorAction SilentlyContinue
"--- repository:"; Get-Content $repo -ErrorAction SilentlyContinue
"--- registry:"; Get-ItemProperty HKCU:\Software\SecondShellProbe -ErrorAction SilentlyContinue | Select-Object * -ExcludeProperty PS* | Format-List
reg.exe delete HKCU\Software\SecondShellProbe /f 2>&1 | Out-Null
if (Test-Path $public) { [IO.File]::Delete($public) }

