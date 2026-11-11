#The AppVeyor build (appveyor.yml): builds Release for x64 and ARM64 (the unit tests run locally), and makes the signed
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
  #The programs which ship and their libraries only: neither the unit tests nor the tools, as the free plan allows an hour
  #a build; the tests run with build.ps1 locally
  & (Join-Path $root "build.ps1") -Configurations Release -Platforms x64, ARM64 -SkipTests -Targets "SecondShell;SecondShell_Mcp;SecondShell_Driver"
  if (-not $?) { throw "The build failed." }

  #The Store's upload too, once the project's variables have the identity Partner Center reserved (Packaging\Store.md)
  & (Join-Path $root "package.ps1") -Platforms x64, ARM64 -SkipBuild -Revision $revision -ReleaseTag $tag `
    -StoreName "$env:SECONDSHELL_STORE_NAME" -StorePublisher "$env:SECONDSHELL_STORE_PUBLISHER" -StorePublisherDisplayName "$env:SECONDSHELL_STORE_PUBLISHER_DISPLAY_NAME"
  if (-not $?) { throw "Packaging failed." }
}
finally
{
  #The private key goes as soon as it is not needed
  if ($env:SECONDSHELL_CERT_PFX) { Remove-Item $certificate -ErrorAction SilentlyContinue }
}
