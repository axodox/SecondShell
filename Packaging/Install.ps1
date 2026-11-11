#Installs SecondShell from its MSIX package or bundle, signed with the self-signed CN=Axodox certificate: trusts the
#certificate on this PC once (one administrator prompt), then installs the package for the current user. Run it from the
#folder with the package and SecondShell.cer, or name the package:
#  powershell -ExecutionPolicy Bypass -File .\Install.ps1
#  powershell -ExecutionPolicy Bypass -File .\Install.ps1 -Package .\SecondShell-0.1.0.msixbundle
param(
  [string]$Package = "",
  [switch]$TrustOnly  #used by the elevated copy of this script
)

$ErrorActionPreference = "Stop"
$certificateFile = Join-Path $PSScriptRoot "SecondShell.cer"
if (-not (Test-Path $certificateFile)) { throw "SecondShell.cer is missing; it belongs next to this script." }
$certificate = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $certificateFile

#Trusted for packages on this PC: in the local machine's Trusted People store, which needs administrator rights
$isTrusted = [bool](Get-ChildItem Cert:\LocalMachine\TrustedPeople | Where-Object Thumbprint -eq $certificate.Thumbprint)
if ($TrustOnly)
{
  if (-not $isTrusted) { Import-Certificate -FilePath $certificateFile -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null }
  return
}

if (-not $isTrusted)
{
  Write-Host "Trusting the SecondShell certificate ($($certificate.Subject), valid until $($certificate.NotAfter.ToString('yyyy-MM-dd'))): approve the administrator prompt." -ForegroundColor Magenta
  $process = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`"", "-TrustOnly")
  if ($process.ExitCode -ne 0 -or -not (Get-ChildItem Cert:\LocalMachine\TrustedPeople | Where-Object Thumbprint -eq $certificate.Thumbprint))
  {
    throw "The certificate was not trusted, so Windows refuses the package."
  }
}

#The bundle, else this PC's package
if (-not $Package)
{
  $Package = Get-ChildItem $PSScriptRoot -Filter "SecondShell-*.msixbundle" | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
  if (-not $Package)
  {
    $architecture = if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "ARM64" } else { "x64" }
    $Package = Get-ChildItem $PSScriptRoot -Recurse -Filter "SecondShell-*-$architecture.msix" | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
  }
  if (-not $Package) { throw "No SecondShell package next to this script." }
}

Write-Host "Installing $(Split-Path $Package -Leaf)..." -ForegroundColor Magenta
Add-AppxPackage -Path $Package -ForceUpdateFromAnyVersion
Write-Host "Done: SecondShell is in the Start menu, and SecondShell.exe and SecondShell.Mcp.exe run from any prompt." -ForegroundColor Green
