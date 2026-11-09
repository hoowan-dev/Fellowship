# Copyright HoowanDev 2026 all rights reserved.

<#
.SYNOPSIS
    Packages the Fellowship Tests extension in Scripts\VSCodeTests and installs it into VS Code.

.DESCRIPTION
    The extension lists the project's automation tests in VS Code's Testing view and runs or debugs them headless
    through UnrealEditor-Cmd. Run this again after changing the extension, then reload the VS Code window.
    Packaging uses @vscode/vsce through npx, so Node.js has to be installed.

.EXAMPLE
    .\Scripts\InstallTestExtension.ps1
#>

$ErrorActionPreference = "Stop"

$ExtensionDir = Join-Path $PSScriptRoot "VSCodeTests"
$Package = Join-Path ([System.IO.Path]::GetTempPath()) "fellowship-tests.vsix"

Push-Location $ExtensionDir
try {
    & npx --yes "@vscode/vsce@3.2.1" package --allow-missing-repository --skip-license --out $Package
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Packaging the extension failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Pop-Location
}

& code --install-extension $Package --force
if ($LASTEXITCODE -ne 0) {
    Write-Error "Installing the extension failed (exit code $LASTEXITCODE)."
}
Remove-Item $Package

Write-Host "Installed. Reload the VS Code window (Developer: Reload Window) to pick it up."
