# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Fellowship is an Unreal Engine 5.7 C++ project (created from the Blank template, `TP_Blank` → `Fellowship` redirects in `Config/DefaultEngine.ini`). The engine is an installed (launcher) build at `C:\Program Files\Epic Games\UE_5.7` — treat it as read-only reference; never modify engine files.

The project is currently at its starting point: a single runtime module (`Source/Fellowship`) with only the default `IMPLEMENT_PRIMARY_GAME_MODULE` boilerplate, and no game code or project assets yet. The default map is the engine's `/Engine/Maps/Templates/OpenWorld` (World Partition).

## Build

Builds go through UnrealBuildTool via the engine's `Build.bat`, run from the engine root. The editor target is the one used day-to-day:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" FellowshipEditor Win64 Development "C:\dev\Fellowship\Fellowship.uproject" -waitmutex
```

- Targets: `FellowshipEditor` (editor) and `Fellowship` (standalone game). Configurations: `Debug`, `DebugGame`, `Development`, `Test`, `Shipping`.
- Clean: `Engine\Build\BatchFiles\Clean.bat` with the same arguments. Rebuild: `Rebuild.bat`.
- The same commands exist as VS Code tasks in `Fellowship.code-workspace` (e.g. "FellowshipEditor Win64 Development Build").
- If the editor is open with Live Coding enabled, a command-line build of the editor target will fail; compile from the editor (Ctrl+Alt+F11) or close it first.
- After adding/removing source files or modules, regenerate project files / compile commands:
  ```powershell
  & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="C:\dev\Fellowship\Fellowship.uproject" -game -vscode
  ```
- Toolchain: MSVC from Visual Studio 18 Community, C++20. IntelliSense uses `.vscode/compileCommands_*.json` (generated; don't hand-edit).

Run the editor: `"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "C:\dev\Fellowship\Fellowship.uproject"`

There are no tests yet. If Automation tests are added, run headless with:
`UnrealEditor-Cmd.exe "C:\dev\Fellowship\Fellowship.uproject" -ExecCmds="Automation RunTests <TestName>;Quit" -unattended -nullrhi -nopause`

## Structure

- `Fellowship.uproject` — module list and plugins (`ModelingToolsEditorMode`, editor-only).
- `Source/Fellowship.Target.cs`, `Source/FellowshipEditor.Target.cs` — build targets (BuildSettings V6, include order `Unreal5_7`). New modules must be added to `ExtraModuleNames` here and to `Modules` in the `.uproject`.
- `Source/Fellowship/Fellowship.Build.cs` — module deps. Public deps already include `EnhancedInput`; Slate and OnlineSubsystem are present as commented-out lines.
- `Config/Default*.ini` — project settings. Rendering is configured for Lumen GI/reflections, hardware ray tracing, Virtual Shadow Maps, Substrate materials, DX12/SM6, no static lighting.
- `Content/` — binary `.uasset`/`.umap` files; edit through the editor, not as text.
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` — generated; ignore. Logs are in `Saved/Logs/`.
