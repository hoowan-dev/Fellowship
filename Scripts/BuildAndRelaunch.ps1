# Copyright HoowanDev 2026 all rights reserved.

<#
.SYNOPSIS
    Closes the Fellowship editor, builds the editor target, and reopens the project.

.DESCRIPTION
    The editor is asked to close normally, so it shows its usual "save changes?" prompt.
    If it hasn't exited within -CloseTimeout seconds (e.g. the prompt was cancelled), the
    script stops without building. The editor is only reopened if the build succeeds.

.EXAMPLE
    .\Scripts\BuildAndRelaunch.ps1
    .\Scripts\BuildAndRelaunch.ps1 -NoRelaunch
#>
param(
    # Seconds to wait for the editor to exit after asking it to close
    [int]$CloseTimeout = 120,

    # Build only; don't reopen the editor afterwards
    [switch]$NoRelaunch
)

$ErrorActionPreference = "Stop"

$EngineDir = "C:\Program Files\Epic Games\UE_5.9"
$ProjectFile = (Resolve-Path (Join-Path $PSScriptRoot "..\Fellowship.uproject")).Path
$BuildBat = Join-Path $EngineDir "Engine\Build\BatchFiles\Build.bat"
$EditorExe = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor.exe"

# --- Close the editor -------------------------------------------------------

# Only editors that have this project open
$ProjectName = [System.IO.Path]::GetFileName($ProjectFile)
$Editors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
    Where-Object { $_.CommandLine -like "*$ProjectName*" })

if ($Editors.Count -gt 0) {
    Write-Host "Closing editor (save any changes when prompted)..."
    foreach ($Editor in $Editors) {
        $Process = Get-Process -Id $Editor.ProcessId -ErrorAction SilentlyContinue
        if ($Process) {
            [void]$Process.CloseMainWindow()
        }
    }

    foreach ($Editor in $Editors) {
        $Process = Get-Process -Id $Editor.ProcessId -ErrorAction SilentlyContinue
        if ($Process -and -not $Process.WaitForExit($CloseTimeout * 1000)) {
            Write-Error "Editor (PID $($Editor.ProcessId)) didn't close within $CloseTimeout seconds. Close it manually and run this script again."
        }
    }
}
else {
    Write-Host "Editor isn't running."
}

# Live Coding's console can outlive the editor briefly and blocks command-line builds
Get-Process -Name "LiveCodingConsole" -ErrorAction SilentlyContinue | ForEach-Object {
    if (-not $_.WaitForExit(10000)) {
        Stop-Process -Id $_.Id -Force
    }
}

# --- Build ------------------------------------------------------------------

Write-Host "Building FellowshipEditor Win64 Development..."
& $BuildBat FellowshipEditor Win64 Development $ProjectFile -waitmutex
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed (exit code $LASTEXITCODE). Not reopening the editor."
}

# --- Relaunch ---------------------------------------------------------------

if (-not $NoRelaunch) {
    Write-Host "Opening editor..."
    Start-Process -FilePath $EditorExe -ArgumentList "`"$ProjectFile`""
}
