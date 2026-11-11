#Builds the packages (Design.md, Deployment): per platform, the portable zip (a SecondShell folder with the executables,
#the self-contained Windows App SDK and the Visual C++ runtime, its own binaries signed), a zip of their symbols, and an
#MSIX package of the same folder; then an MSIX bundle of the platforms, and its .appinstaller file for GitHub releases.
#Signed as unpaint is: with Packaging\key.pfx, the self-signed CN=Axodox certificate (valid until 2036-11-11), whose
#password is in the SECONDSHELL_CERT_PW environment variable; without it, with a CN=Axodox code-signing certificate
#from the user's store; without either, unsigned.
#Examples:
#  .\package.ps1                                   (Release, x64 and ARM64, signed, timestamped)
#  .\package.ps1 -Platforms x64 -SkipBuild
#  .\package.ps1 -Platforms x64 -SkipBuild -BinFolder bin\x64\ReleaseWork   (an existing build elsewhere)
#  .\package.ps1 -CertificateSubject "" -CertificateFile ""   (unsigned)
#  .\package.ps1 -StoreName 12345Axodox.SecondShell -StorePublisher "CN=..." -StorePublisherDisplayName Axodox
#                                                  (also artifacts\store\SecondShell-<version>.msixupload)
param(
  [string[]]$Platforms = @("x64", "ARM64"),
  [string]$CertificateFile = (Join-Path $PSScriptRoot "Packaging\key.pfx"),
  [string]$CertificatePassword = $env:SECONDSHELL_CERT_PW,
  [string]$CertificateSubject = "CN=Axodox",
  [string]$TimestampUrl = "http://timestamp.digicert.com",
  [switch]$SkipBuild,
  [string]$BinFolder = "",  #with -SkipBuild and one platform: the build to package, instead of bin\<platform>\Release
  [int]$Revision = 0,       #the package version's fourth part: CI's build number, so each build installs over the last
  [string]$ReleaseTag = "", #the GitHub release the bundle is published in; default v<version>
  #The Store's upload too (Packaging\Store.md), with the identity Partner Center reserved for the app (Product
  #management, Product identity): Package/Identity/Name, Package/Identity/Publisher and Package/Properties/PublisherDisplayName
  [string]$StoreName = "",
  [string]$StorePublisher = "",
  [string]$StorePublisherDisplayName = ""
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
$makePri = Get-ChildItem $kitBin -Filter makepri.exe -Recurse | Where-Object { $_.FullName -match "\\x64\\" } | Sort-Object FullName -Descending | Select-Object -First 1
$packageVersion = "$version.$Revision"
if (-not $ReleaseTag) { $ReleaseTag = "v$version" }

function Sign([string[]]$files)
{
  if (-not $certificate) { return }
  $arguments = @("sign", "/fd", "SHA256")
  if ($pfx) { $arguments += @("/f", $pfx, "/p", $CertificatePassword) }
  else { $arguments += @("/sha1", $certificate.Thumbprint, "/s", "My") }
  if ($TimestampUrl) { $arguments += @("/tr", $TimestampUrl, "/td", "SHA256") }
  & $signTool.FullName @arguments @files | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "Signing failed: $files" }
}

#Packs the staged folder of a platform as an MSIX package of the identity: with the manifest, the logos, and the
#package's resource index, in artifacts\<platform>\<layoutName>
function Pack([string]$platform, [string]$stage, [string]$layoutName, [hashtable]$identity, [string]$packageVersion, [string]$msix)
{
  $layout = Join-Path $artifacts "$platform\$layoutName"
  Copy-Item $stage $layout -Recurse
  Copy-Item (Join-Path $root "Packaging\Assets") (Join-Path $layout "Assets") -Recurse
  (Get-Content (Join-Path $root "Packaging\AppxManifest.xml") -Raw).Replace('$Name$', $identity.Name).Replace('$Publisher$', $identity.Publisher).
    Replace('$PublisherDisplayName$', $identity.PublisherDisplayName).Replace('$Version$', $packageVersion).Replace('$Architecture$', $platform.ToLower()) |
    Set-Content (Join-Path $layout "AppxManifest.xml") -Encoding utf8 -NoNewline

  #resources.pri, which a packaged app's resources are looked up in, under the package's name: the app's own
  #(SecondShell.pri, the window's XAML) merged with the logos, whose size and plate variants
  #(Square44x44Logo.targetsize-NN_altform-unplated) let Start and the taskbar show the mark without a plate behind it
  $priStage = Join-Path $artifacts "$platform\pri"
  if (Test-Path $priStage) { Remove-Item $priStage -Recurse -Force }
  New-Item $priStage -ItemType Directory | Out-Null
  Copy-Item (Join-Path $root "Packaging\Assets") (Join-Path $priStage "Assets") -Recurse
  Copy-Item (Join-Path $layout "SecondShell.pri") $priStage
  $priConfig = Join-Path $artifacts "$platform\priconfig.xml"
  & $makePri.FullName createconfig /cf $priConfig /dq en-US /pv 10.0.0 /o | Out-Null
  [regex]::Replace((Get-Content $priConfig -Raw), '(?s)<packaging>.*?</packaging>\s*', '') | Set-Content $priConfig -Encoding utf8
  $priOutput = & $makePri.FullName new /pr $priStage /cf $priConfig /mn (Join-Path $layout "AppxManifest.xml") /of (Join-Path $layout "resources.pri") /o
  if ($LASTEXITCODE -ne 0) { $priOutput | Out-Host; throw "Indexing the package's resources failed for $platform." }

  New-Item (Split-Path $msix) -ItemType Directory -Force | Out-Null
  & $makeAppx.FullName pack /o /h SHA256 /d $layout /p $msix | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "Packing the MSIX failed for $platform." }
}

#The Store's identity, if all of it is given
$store = $null
if ($StoreName -or $StorePublisher -or $StorePublisherDisplayName)
{
  if (-not ($StoreName -and $StorePublisher -and $StorePublisherDisplayName)) { throw "The Store's upload needs -StoreName, -StorePublisher and -StorePublisherDisplayName." }
  $store = @{ Name = $StoreName; Publisher = $StorePublisher; PublisherDisplayName = $StorePublisherDisplayName }
}

#The certificate file, if its password is known; else the newest valid code-signing certificate with the subject, from
#the user's store
$certificate = $null
$pfx = $null
if ($CertificateFile -and (Test-Path $CertificateFile) -and $CertificatePassword)
{
  $certificate = Get-PfxCertificate -FilePath $CertificateFile -Password (ConvertTo-SecureString $CertificatePassword -AsPlainText -Force)
  $pfx = $CertificateFile
}
elseif ($CertificateFile -and (Test-Path $CertificateFile))
{
  Write-Warning "$CertificateFile needs its password in SECONDSHELL_CERT_PW (or -CertificatePassword); trying the certificate store."
}

if (-not $certificate -and $CertificateSubject)
{
  $certificate = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Where-Object { $_.Subject -eq $CertificateSubject -and $_.NotAfter -gt (Get-Date) -and $_.HasPrivateKey } |
    Sort-Object NotAfter -Descending | Select-Object -First 1
  if (-not $certificate) { Write-Warning "No valid code-signing certificate '$CertificateSubject' in Cert:\CurrentUser\My; the binaries stay unsigned." }
}

#What the portable folder leaves out: build leftovers, symbols (zipped apart), the test projects, and Windows Runtime
#metadata, which C++ programs compile against but do not read at run time (and whose WinUI copies have the same GUIDs as
#Windows' own, which the Store's certification fails)
$excluded = @("*.pdb", "*.exp", "*.lib", "*.ilk", "*.iobj", "*.ipdb", "*.winmd", "SecondShell.Tests.*", "SecondShell.PointerRecorder.*")
$excludedFolders = @("PointerRecords")
$ownBinaries = @("SecondShell.exe", "SecondShell.Driver.exe", "SecondShell.Mcp.exe", "SecondShell.Common.dll", "Axodox.Common.dll")

New-Item $artifacts -ItemType Directory -Force | Out-Null
#The certificate's public part and the script which installs a package with it: a self-signed package installs once
#its certificate is trusted, in Cert:\LocalMachine\TrustedPeople (administrator, once per PC)
if ($certificate)
{
  Export-Certificate -Cert $certificate -FilePath (Join-Path $artifacts "SecondShell.cer") | Out-Null
  Copy-Item (Join-Path $root "Packaging\Install.ps1") $artifacts
}

#The bundles take the packages of this run's platforms only
Remove-Item (Join-Path $artifacts "bundle"), (Join-Path $artifacts "store") -Recurse -Force -ErrorAction SilentlyContinue
if ($store) { New-Item (Join-Path $artifacts "store\upload") -ItemType Directory -Force | Out-Null }
foreach ($platform in $Platforms)
{
  if (-not $SkipBuild)
  {
    & (Join-Path $root "build.ps1") -Configurations Release -Platforms $platform -SkipTests
    if (-not $?) { throw "Building Release $platform failed." }
  }

  $output = if ($BinFolder) { (Resolve-Path $BinFolder).Path } else { Join-Path $root "bin\$platform\Release" }
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

  #The MSIX package, signed for sideloading
  $msix = Join-Path $artifacts "bundle\SecondShell-$version-$platform.msix"
  Pack $platform $stage "msix" @{ Name = "Axodox.SecondShell"; Publisher = "CN=Axodox"; PublisherDisplayName = "Axodox" } $packageVersion $msix
  Sign @($msix)
  Write-Host ("{0}: {1:N1} MB" -f (Split-Path $msix -Leaf), ((Get-Item $msix).Length / 1MB)) -ForegroundColor Green

  #The Store's: unsigned (the Store signs it), and versioned <version>.0, as the Store keeps the fourth part for itself;
  #with its symbols (an .appxsym is a zip of the .pdb files), which the Store's crash reports are resolved with
  if ($store)
  {
    Pack $platform $stage "store-msix" $store "$version.0" (Join-Path $artifacts "store\packages\SecondShell-$version-$platform.msix")
    Copy-Item $symbols (Join-Path $artifacts "store\upload\SecondShell-$version-$platform.appxsym")
  }
}

#The bundle of the platforms' packages, and the .appinstaller file which installs and updates it from the releases
$bundle = Join-Path $artifacts "SecondShell-$version.msixbundle"
& $makeAppx.FullName bundle /o /bv $packageVersion /d (Join-Path $artifacts "bundle") /p $bundle | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Bundling the MSIX packages failed." }
Sign @($bundle)

$appInstaller = Join-Path $artifacts "SecondShell.appinstaller"
(Get-Content (Join-Path $root "Packaging\SecondShell.appinstaller") -Raw).Replace('$Version$', $packageVersion).Replace('$Bundle$', "$releases/download/$ReleaseTag/SecondShell-$version.msixbundle").Replace('$Self$', "$releases/latest/download/SecondShell.appinstaller") |
  Set-Content $appInstaller -Encoding utf8 -NoNewline
Write-Host ("{0}: {1:N1} MB, and {2}" -f (Split-Path $bundle -Leaf), ((Get-Item $bundle).Length / 1MB), (Split-Path $appInstaller -Leaf)) -ForegroundColor Green

#The Store's upload: a zip of the unsigned bundle and the platforms' symbols, which Partner Center takes as is
if ($store)
{
  $storeBundle = Join-Path $artifacts "store\upload\SecondShell-$version.msixbundle"
  & $makeAppx.FullName bundle /o /bv "$version.0" /d (Join-Path $artifacts "store\packages") /p $storeBundle | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "Bundling the Store's packages failed." }

  $upload = Join-Path $artifacts "store\SecondShell-$version.msixupload"
  Compress-Archive -Path (Join-Path $artifacts "store\upload\*") -DestinationPath "$upload.zip" -CompressionLevel Optimal
  Move-Item "$upload.zip" $upload -Force
  Write-Host ("{0}: {1:N1} MB, for Partner Center" -f (Split-Path $upload -Leaf), ((Get-Item $upload).Length / 1MB)) -ForegroundColor Green
}

Write-Host "Done." -ForegroundColor Green
