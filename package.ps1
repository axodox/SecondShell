#Builds the packages (Design.md, Deployment): per platform, the portable zip (a SecondShell folder with the executables,
#the self-contained Windows App SDK and the Visual C++ runtime, its own binaries signed), a zip of their symbols, and an
#MSIX package of the same folder; then an MSIX bundle of the platforms, and its .appinstaller file for GitHub releases.
#Examples:
#  .\package.ps1                                   (Release, x64 and ARM64, signed with the CN=Axodox certificate)
#  .\package.ps1 -Platforms x64 -SkipBuild
#  .\package.ps1 -CertificateSubject ""            (unsigned)
param(
  [string[]]$Platforms = @("x64", "ARM64"),
  [string]$CertificateSubject = "CN=Axodox",
  [string]$TimestampUrl = "",
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$artifacts = Join-Path $root "artifacts"

#The version, as the binaries have it
$props = Get-Content (Join-Path $root "Build\SecondShell.Cpp.props") -Raw
$version = [regex]::Match($props, "<SecondShellVersion>([^<]+)</SecondShellVersion>").Groups[1].Value
if (-not $version) { throw "No SecondShellVersion in Build\SecondShell.Cpp.props." }

#The repository the releases are published in, for the .appinstaller file
$releases = "https://github.com/axodox/SecondShell/releases"

#Tools
$visualStudio = & (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe") -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$kitBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
$signTool = Get-ChildItem $kitBin -Filter signtool.exe -Recurse | Where-Object { $_.FullName -match "\\x64\\" } | Sort-Object FullName -Descending | Select-Object -First 1
$makeAppx = Get-ChildItem $kitBin -Filter makeappx.exe -Recurse | Where-Object { $_.FullName -match "\\x64\\" } | Sort-Object FullName -Descending | Select-Object -First 1
$packageVersion = "$version.0"

function Sign([string[]]$files)
{
  if (-not $certificate) { return }
  $arguments = @("sign", "/fd", "SHA256", "/sha1", $certificate.Thumbprint, "/s", "My")
  if ($TimestampUrl) { $arguments += @("/tr", $TimestampUrl, "/td", "SHA256") }
  & $signTool.FullName @arguments @files | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "Signing failed: $files" }
}

#The newest valid code-signing certificate with the subject, from the user's store
$certificate = $null
if ($CertificateSubject)
{
  $certificate = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Where-Object { $_.Subject -eq $CertificateSubject -and $_.NotAfter -gt (Get-Date) -and $_.HasPrivateKey } |
    Sort-Object NotAfter -Descending | Select-Object -First 1
  if (-not $certificate) { Write-Warning "No valid code-signing certificate '$CertificateSubject' in Cert:\CurrentUser\My; the binaries stay unsigned." }
}

#What the portable folder leaves out: build leftovers, symbols (zipped apart) and the test projects
$excluded = @("*.pdb", "*.exp", "*.lib", "*.ilk", "*.iobj", "*.ipdb", "SecondShell.Tests.*", "SecondShell.PointerRecorder.*")
$excludedFolders = @("PointerRecords")
$ownBinaries = @("SecondShell.exe", "SecondShell.Driver.exe", "SecondShell.Mcp.exe", "SecondShell.Common.dll", "Axodox.Common.dll")

New-Item $artifacts -ItemType Directory -Force | Out-Null
#The certificate's public part, for installing the self-signed packages: trust it once (administrator), in
#Cert:\LocalMachine\TrustedPeople
if ($certificate) { Export-Certificate -Cert $certificate -FilePath (Join-Path $artifacts "SecondShell.cer") | Out-Null }

#The bundle takes the packages of this run's platforms only
Remove-Item (Join-Path $artifacts "bundle") -Recurse -Force -ErrorAction SilentlyContinue
foreach ($platform in $Platforms)
{
  if (-not $SkipBuild)
  {
    & (Join-Path $root "build.ps1") -Configurations Release -Platforms $platform -SkipTests
    if (-not $?) { throw "Building Release $platform failed." }
  }

  $output = Join-Path $root "bin\$platform\Release"
  $stage = Join-Path $artifacts "$platform\SecondShell"
  Write-Host "Staging $platform..." -ForegroundColor Magenta
  if (Test-Path (Split-Path $stage)) { Remove-Item (Split-Path $stage) -Recurse -Force }
  New-Item $stage -ItemType Directory -Force | Out-Null

  Get-ChildItem $output | Where-Object {
    $item = $_
    -not ($excluded | Where-Object { $item.Name -like $_ }) -and -not ($item.PSIsContainer -and $excludedFolders -contains $item.Name)
  } | Copy-Item -Destination $stage -Recurse

  #The Visual C++ runtime, next to the executables (app-local deployment, no installer needed)
  $runtime = Get-ChildItem (Join-Path $visualStudio "VC\Redist\MSVC") -Directory | Sort-Object Name -Descending |
    ForEach-Object { Get-ChildItem (Join-Path $_.FullName $platform.ToLower()) -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } | Select-Object -First 1
  if (-not $runtime) { throw "No Visual C++ runtime for $platform in $visualStudio." }
  Copy-Item (Join-Path $runtime.FullName "*.dll") -Destination $stage

  if ($certificate) { Write-Host "Signing $platform with $($certificate.Subject) ($($certificate.Thumbprint))..." -ForegroundColor Magenta }
  Sign ($ownBinaries | ForEach-Object { Join-Path $stage $_ })

  #The zips: the folder, and the symbols of SecondShell's own binaries
  $zip = Join-Path $artifacts "SecondShell-$version-$platform.zip"
  Remove-Item $zip -ErrorAction SilentlyContinue
  Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal

  $symbols = Join-Path $artifacts "SecondShell-$version-$platform-symbols.zip"
  Remove-Item $symbols -ErrorAction SilentlyContinue
  $pdbs = $ownBinaries | ForEach-Object { Join-Path $output ([IO.Path]::ChangeExtension($_, ".pdb")) } | Where-Object { Test-Path $_ }
  Compress-Archive -Path $pdbs -DestinationPath $symbols -CompressionLevel Optimal

  Write-Host ("{0}: {1:N1} MB" -f (Split-Path $zip -Leaf), ((Get-Item $zip).Length / 1MB)) -ForegroundColor Green

  #The MSIX package: the same folder, with the manifest, the logos, and the app's resource index under the name a
  #packaged app's is looked up by
  $layout = Join-Path $artifacts "$platform\msix"
  Copy-Item $stage $layout -Recurse
  Copy-Item (Join-Path $root "Packaging\Assets") (Join-Path $layout "Assets") -Recurse
  Copy-Item (Join-Path $layout "SecondShell.pri") (Join-Path $layout "resources.pri")
  (Get-Content (Join-Path $root "Packaging\AppxManifest.xml") -Raw).Replace('$Version$', $packageVersion).Replace('$Architecture$', $platform.ToLower()) |
    Set-Content (Join-Path $layout "AppxManifest.xml") -Encoding utf8 -NoNewline

  $msix = Join-Path $artifacts "bundle\SecondShell-$version-$platform.msix"
  New-Item (Split-Path $msix) -ItemType Directory -Force | Out-Null
  & $makeAppx.FullName pack /o /h SHA256 /d $layout /p $msix | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "Packing the MSIX failed for $platform." }
  Sign @($msix)
  Write-Host ("{0}: {1:N1} MB" -f (Split-Path $msix -Leaf), ((Get-Item $msix).Length / 1MB)) -ForegroundColor Green
}

#The bundle of the platforms' packages, and the .appinstaller file which installs and updates it from the releases
$bundle = Join-Path $artifacts "SecondShell-$version.msixbundle"
& $makeAppx.FullName bundle /o /bv $packageVersion /d (Join-Path $artifacts "bundle") /p $bundle | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Bundling the MSIX packages failed." }
Sign @($bundle)

$appInstaller = Join-Path $artifacts "SecondShell.appinstaller"
(Get-Content (Join-Path $root "Packaging\SecondShell.appinstaller") -Raw).Replace('$Version$', $packageVersion).Replace('$Bundle$', "$releases/download/v$version/SecondShell-$version.msixbundle").Replace('$Self$', "$releases/latest/download/SecondShell.appinstaller") |
  Set-Content $appInstaller -Encoding utf8 -NoNewline
Write-Host ("{0}: {1:N1} MB, and {2}" -f (Split-Path $bundle -Leaf), ((Get-Item $bundle).Length / 1MB), (Split-Path $appInstaller -Leaf)) -ForegroundColor Green

Write-Host "Done." -ForegroundColor Green
