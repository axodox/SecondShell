#Runs one stage 1 spike through RdpHost.exe and collects its logs into Spikes\results\<Label>.
#RdpHost is started through Explorer (Breakaway.exe --explorer), so it runs outside any MSIX package context of the
#calling terminal; otherwise its RunOnce entry and files may be redirected into the package (see Findings.md).
#Examples:
#  .\run-spike.ps1 -Label s1-ui-never -HostArgs '--thread','ui','--window','never' -Program SessionCheck -ProgramArgs '--seconds','60'
#  .\run-spike.ps1 -Label s2-nvidia -HostArgs '--frames','--adapter','0' -Program SharedFrames -ProgramArgs '--seconds','30'
param(
  [Parameter(Mandatory)] [string]$Label,
  [string[]]$HostArgs = @(),
  [string]$Program,
  [string[]]$ProgramArgs = @(),
  [int]$Timeout = 180,
  [string]$Configuration = "Release",
  [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$bin = Join-Path $PSScriptRoot "bin\$Platform\$Configuration"
$out = Join-Path $PSScriptRoot "exchange"
$results = Join-Path $PSScriptRoot "results\$Label"

New-Item $out -ItemType Directory -Force | Out-Null
Get-ChildItem $out -File | Remove-Item -Force

$arguments = @("--label", $Label, "--timeout", $Timeout) + $HostArgs
if ($Program)
{
  $exe = Join-Path $bin "$Program\$Program.exe"
  $run = "`"$exe`" " + ($ProgramArgs -join " ")
  $arguments += @("--run", "`"$($run.Replace('"', '\"'))`"")
}

$hostExe = Join-Path $bin "RdpHost\RdpHost.exe"
$breakaway = Join-Path $bin "Breakaway\Breakaway.exe"
Write-Host "RdpHost $($arguments -join ' ')" -ForegroundColor Magenta
$argumentsFile = Join-Path $out "host-arguments.txt"
[IO.File]::WriteAllText($argumentsFile, ($arguments -join " "))
& $breakaway --explorer $hostExe "@$argumentsFile"
if ($LASTEXITCODE -ne 0) { throw "Starting RdpHost through Explorer failed ($LASTEXITCODE)" }

$hostLog = Join-Path $out "host.log"
$deadline = (Get-Date).AddSeconds($Timeout + 120)
while ((Get-Date) -lt $deadline -and -not ((Test-Path $hostLog) -and (Select-String -Path $hostLog -Pattern "HOST DONE" -Quiet))) { Start-Sleep -Seconds 1 }
if ((Get-Date) -ge $deadline) { Write-Warning "RdpHost did not finish in time" }
Start-Sleep -Seconds 2

New-Item $results -ItemType Directory -Force | Out-Null
Get-ChildItem $out -File | Where-Object Extension -in ".log", ".png", ".txt" | Copy-Item -Destination $results -Force
Write-Host "Results in $results" -ForegroundColor Green
qwinsta

