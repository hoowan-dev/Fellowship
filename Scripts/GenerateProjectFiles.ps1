# Copyright HoowanDev 2026 all rights reserved.

<#
.SYNOPSIS
    Regenerates the VS Code project files, then adds the workspace settings the generator leaves out.

.DESCRIPTION
    UnrealBuildTool rewrites Fellowship.code-workspace from scratch every time, so anything added to it by hand is lost.
    This runs the generator and then merges in settings that stop extensions complaining when the workspace opens:
      - The C# extension is told not to load any projects. The workspace includes the engine, whose C# projects it
        tries and fails to load, and the project's own C# is only build rules, which UnrealBuildTool compiles.
      - The Java extensions are told not to import Gradle or Maven projects, which the engine has for Android libraries.
    Run it instead of the bare generator after adding or removing source files or modules.

.EXAMPLE
    .\Scripts\GenerateProjectFiles.ps1
#>

$ErrorActionPreference = "Stop"

$EngineDir = "C:\Program Files\Epic Games\UE_5.9"
$ProjectFile = (Resolve-Path (Join-Path $PSScriptRoot "..\Fellowship.uproject")).Path
$WorkspaceFile = Join-Path (Split-Path $ProjectFile) "Fellowship.code-workspace"
$BuildBat = Join-Path $EngineDir "Engine\Build\BatchFiles\Build.bat"

Write-Host "Generating VS Code project files..."
& $BuildBat -projectfiles -project="$ProjectFile" -game -vscode
if ($LASTEXITCODE -ne 0) {
    Write-Error "Project file generation failed (exit code $LASTEXITCODE)."
}

$ExtraSettings = [ordered]@{
    "dotnet.defaultSolution"          = "disable"
    "java.import.gradle.enabled"      = $false
    "java.import.maven.enabled"       = $false
    "java.gradle.buildServer.enabled" = "off"
    "gradle.autoDetect"               = "off"
}

$Workspace = Get-Content -Raw -Path $WorkspaceFile | ConvertFrom-Json
foreach ($Setting in $ExtraSettings.GetEnumerator()) {
    $Workspace.settings | Add-Member -NotePropertyName $Setting.Key -NotePropertyValue $Setting.Value -Force
}

# Without BOM, which some JSON readers reject. ConvertTo-Json only goes two levels deep unless told otherwise, and the launch configurations go further.
$Json = $Workspace | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($WorkspaceFile, $Json, (New-Object System.Text.UTF8Encoding $false))

Write-Host "Added workspace settings: $($ExtraSettings.Keys -join ', ')"
