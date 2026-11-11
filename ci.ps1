#The AppVeyor build (appveyor.yml): builds Release for x64 and ARM64 (the unit tests run locally), and makes the signed
#packages in artifacts (package.ps1), versioned with the build number. The signing certificate comes from the project's
#secure variables: SECONDSHELL_CERT_PFX (Packaging\key.pfx, base64) and SECONDSHELL_CERT_PW (its password); without
#them the packages are unsigned.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$props = Get-Content (Join-Path $root "Build\SecondShell.Cpp.props") -Raw
$version = [regex]::Match($props, "<SecondShellVersion>([^<]+)</SecondShellVersion>").Groups[1].Value
$revision = if ($env:APPVEYOR_BUILD_NUMBER) { [int]$env:APPVEYOR_BUILD_NUMBER } else { 0 }
#master's builds are releases, tagged v<version>.<build>; other branches' are prereleases, tagged with the branch too
#(the .appinstaller file's update address, latest/download, finds releases only)
$branch = "$env:APPVEYOR_REPO_BRANCH"
$isRelease = (-not $branch) -or $branch -eq "master"
$tag = if ($isRelease) { "v$version.$revision" } else { "v$version.$revision-" + ($branch -replace "[^A-Za-z0-9.-]", "-") }

#The version and the release's tag and kind, for the deployment and the build's name
if ($env:APPVEYOR)
{
  Update-AppveyorBuild -Version "$version.$revision"
  Set-AppveyorBuildVariable -Name "SECONDSHELL_RELEASE_TAG" -Value $tag
  Set-AppveyorBuildVariable -Name "SECONDSHELL_RELEASE_KIND" -Value $(if ($isRelease) { "release" } else { "prerelease" })
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

  #What a GitHub release gets, in one folder (appveyor.yml's Packages artifact)
  $release = Join-Path $root "artifacts\release"
  New-Item $release -ItemType Directory -Force | Out-Null
  Copy-Item (Join-Path $root "artifacts\*") $release -Include "SecondShell-*.msixbundle", "SecondShell-*.zip", "SecondShell.appinstaller", "SecondShell.cer", "Install.ps1"
}
finally
{
  #The private key goes as soon as it is not needed
  if ($env:SECONDSHELL_CERT_PFX) { Remove-Item $certificate -ErrorAction SilentlyContinue }
}
