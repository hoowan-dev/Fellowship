# Copyright HoowanDev 2026 all rights reserved.

<#
.SYNOPSIS
    Opens the Fellowship project in the editor.

.EXAMPLE
    .\Scripts\Launch.ps1
#>

$ErrorActionPreference = "Stop"

$EngineDir = "C:\Program Files\Epic Games\UE_5.7"
$ProjectFile = (Resolve-Path (Join-Path $PSScriptRoot "..\Fellowship.uproject")).Path
$EditorExe = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor.exe"

Write-Host "Opening editor..."
Start-Process -FilePath $EditorExe -ArgumentList "`"$ProjectFile`""
