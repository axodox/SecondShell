# SecondShell

SecondShell gives AI agents their own Windows desktop to work in: a second session of the same user, controlled through an MCP server, without disturbing the user's own desktop.

## Documentation

- [docs/Design.md](docs/Design.md): what SecondShell does, its MCP tools and actions.
- [docs/Findings.md](docs/Findings.md): verified behaviour of the Windows features it is built on.
- [docs/Architecture.md](docs/Architecture.md): projects, classes and their public APIs.
- [docs/ImplementationPlan.md](docs/ImplementationPlan.md): the implementation stages.

## Building

Requirements: Visual Studio 2026 (18.x) with the C++ desktop workload, MSVC v145 build tools for x64 and ARM64, and Windows SDK 10.0.26100.

```powershell
.\build.ps1                                                       #Debug x64, with unit tests
.\build.ps1 -Configurations Debug,Release -Platforms x64,ARM64    #everything
```

`build.ps1` downloads `nuget.exe` into `.tools`, restores the packages listed in `Build\packages.config` into `packages`, builds the solution and runs the unit tests. After the first restore, the solution can also be built from Visual Studio. All executables are written to `bin\<platform>\<configuration>`.

### Building against a local Axodox.Common

To develop Axodox.Common alongside SecondShell, create `Build\local.props` (ignored by git) pointing to the local repository:

```xml
<Project>
  <PropertyGroup>
    <AxodoxCommon-Location>G:\dev\axodox-common\Axodox.Common.Desktop</AxodoxCommon-Location>
  </PropertyGroup>
</Project>
```