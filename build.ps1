#Restores packages, builds the solution and runs the unit tests.
#Examples:
#  .\build.ps1                                      (Debug x64, with tests)
#  .\build.ps1 -Configurations Debug,Release -Platforms x64,ARM64
#  .\build.ps1 -SessionTests                       (only the tests that sign in to the child session)
param(
  [string[]]$Configurations = @("Debug"),
  [string[]]$Platforms = @("x64"),
  [switch]$SkipTests,
  [switch]$SessionTests
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

#Tools
$nuget = Join-Path $root ".tools\nuget.exe"
if (-not (Test-Path $nuget))
{
  New-Item (Split-Path $nuget) -ItemType Directory -Force | Out-Null
  Invoke-WebRequest "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" -OutFile $nuget -UseBasicParsing
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$visualStudio = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $visualStudio "MSBuild\Current\Bin\amd64\MSBuild.exe"
$vstest = Join-Path $visualStudio "Common7\IDE\Extensions\TestPlatform\vstest.console.exe"

#Packages
Write-Host "Restoring packages..." -ForegroundColor Magenta
& $nuget restore (Join-Path $root "Build\packages.config") -PackagesDirectory (Join-Path $root "packages") -Source https://api.nuget.org/v3/index.json -NonInteractive -Verbosity quiet
if ($LASTEXITCODE -ne 0) { throw "Package restore failed." }

#Build
foreach ($platform in $Platforms)
{
  foreach ($configuration in $Configurations)
  {
    Write-Host "Building $configuration $platform..." -ForegroundColor Magenta
    #The 64-bit compilers for every platform, also for ARM64, whose default is the 32-bit ones
    & $msbuild (Join-Path $root "SecondShell.sln") -p:Configuration=$configuration -p:Platform=$platform -p:PreferredToolArchitecture=x64 -m -v:m -nologo
    if ($LASTEXITCODE -ne 0) { throw "Building $configuration $platform failed." }
  }
}

#Tests (x64 only: ARM64 binaries cannot run on an x64 machine)
if (-not $SkipTests -and $Platforms -contains "x64")
{
  foreach ($configuration in $Configurations)
  {
    Write-Host "Testing $configuration x64..." -ForegroundColor Magenta
    #The session tests need the saved password and log off the child session, so they run only when asked for
    $filter = if ($SessionTests) { "FullyQualifiedName~SessionProbeTests" } else { "FullyQualifiedName!~SessionProbeTests" }
    & $vstest (Join-Path $root "bin\x64\$configuration\SecondShell.Tests.dll") /Platform:x64 /TestCaseFilter:$filter
    if ($LASTEXITCODE -ne 0) { throw "Tests failed for $configuration x64." }
  }
}

Write-Host "Done." -ForegroundColor Green