#The AppVeyor build (appveyor.yml): builds Release for x64 and ARM64, runs the unit tests (x64), and makes the signed
#packages in artifacts (package.ps1), versioned with the build number. The signing certificate comes from the project's
#secure variables: SECONDSHELL_CERT_PFX (Packaging\key.pfx, base64) and SECONDSHELL_CERT_PW (its password); without
#them the packages are unsigned.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$props = Get-Content (Join-Path $root "Build\SecondShell.Cpp.props") -Raw
$version = [regex]::Match($props, "<SecondShellVersion>([^<]+)</SecondShellVersion>").Groups[1].Value
$revision = if ($env:APPVEYOR_BUILD_NUMBER) { [int]$env:APPVEYOR_BUILD_NUMBER } else { 0 }
$tag = "v$version.$revision"

#The version and the release's tag, for the deployment and the build's name
if ($env:APPVEYOR)
{
  Update-AppveyorBuild -Version "$version.$revision"
  Set-AppveyorBuildVariable -Name "SECONDSHELL_RELEASE_TAG" -Value $tag
}

$certificate = Join-Path $root "Packaging\key.pfx"
if ($env:SECONDSHELL_CERT_PFX)
{
  [IO.File]::WriteAllBytes($certificate, [Convert]::FromBase64String($env:SECONDSHELL_CERT_PFX))
}

try
{
  & (Join-Path $root "build.ps1") -Configurations Release -Platforms x64, ARM64
  if (-not $?) { throw "The build failed." }

  & (Join-Path $root "package.ps1") -Platforms x64, ARM64 -SkipBuild -Revision $revision -ReleaseTag $tag
  if (-not $?) { throw "Packaging failed." }
}
finally
{
  #The private key goes as soon as it is not needed
  if ($env:SECONDSHELL_CERT_PFX) { Remove-Item $certificate -ErrorAction SilentlyContinue }
}
