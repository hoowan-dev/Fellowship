# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Fellowship is an Unreal Engine 5.9 C++ project (created from the Blank template, `TP_Blank` → `Fellowship` redirects in `Config/DefaultEngine.ini`). The engine is an installed preview build at `C:\Program Files\Epic Games\UE_5.9` — treat it as read-only reference; never modify engine files.

The project is currently at its starting point: a single runtime module (`Source/Fellowship`) with only the default `IMPLEMENT_PRIMARY_GAME_MODULE` boilerplate, and no game code or project assets yet. The default map is the engine's `/Engine/Maps/Templates/OpenWorld` (World Partition).

## Build

Builds go through UnrealBuildTool via the engine's `Build.bat`, run from the engine root. The editor target is the one used day-to-day:

```powershell
& "C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\Build.bat" FellowshipEditor Win64 Development "C:\dev\Fellowship\Fellowship.uproject" -waitmutex
```

- Targets: `FellowshipEditor` (editor) and `Fellowship` (standalone game). Configurations: `Debug`, `DebugGame`, `Development`, `Test`, `Shipping`.
- Clean: `Engine\Build\BatchFiles\Clean.bat` with the same arguments. Rebuild: `Rebuild.bat`.
- The same commands exist as VS Code tasks in `Fellowship.code-workspace` (e.g. "FellowshipEditor Win64 Development Build").
- If the editor is open with Live Coding enabled, a command-line build of the editor target will fail; compile from the editor (Ctrl+Alt+F11) or close it first. `Scripts\BuildAndRelaunch.ps1` closes the editor (with its save prompt), builds Development, and reopens the project.
- After adding/removing source files or modules, regenerate project files / compile commands with `Scripts\GenerateProjectFiles.ps1`. It runs `Build.bat -projectfiles -game -vscode` and then adds the workspace settings the generator leaves out, which turn off the C# and Java extensions' project loading, since it fails on the engine folder. Running the generator directly, or the editor's "Refresh Visual Studio Code Project", drops those settings again.
- F5 in VS Code runs "Debug Fellowship Editor (DebugGame)" from `.vscode/launch.json` and `.vscode/tasks.json`, which are hand-written and survive regeneration. An installed engine has no Debug libraries, so the generated "(Debug)" configurations can't build. The UE5 Dev Tools extension is disabled for this workspace: on opening a project it regenerates the project files and overwrites `.vscode/launch.json`, `tasks.json`, `c_cpp_properties.json` and `settings.json`, and it reads its engine path from `ue5devtools.enginePath` rather than the `ue5.enginePath` it declares.
- Toolchain: MSVC from Visual Studio 18 Community, C++20. IntelliSense uses `.vscode/compileCommands_*.json` (generated; don't hand-edit).

Run the editor: `"C:\Program Files\Epic Games\UE_5.9\Engine\Binaries\Win64\UnrealEditor.exe" "C:\dev\Fellowship\Fellowship.uproject"`

Automation tests live in `Source/Fellowship/Tests/`. Run them headless (`Fellowship` runs all of them) with:
`UnrealEditor-Cmd.exe "C:\dev\Fellowship\Fellowship.uproject" -ExecCmds="Automation RunTests <TestName>;Quit" -unattended -nullrhi -nopause`

The tests also show in VS Code's Testing view through a small local extension in `Scripts/VSCodeTests/`. Install or update it with `Scripts\InstallTestExtension.ps1`, then reload the window. It finds tests by reading the `IMPLEMENT_*_AUTOMATION_TEST`/`DEFINE_SPEC` macros in `Source/`. Before each run it builds the editor target: Development to run, DebugGame to debug (under `cppvsdbg`). It then runs the selected tests headless with `^Path$`/`^Group.` filters and reads the results from `Saved/Automation/VSCode/index.json`. If the editor is open with Live Coding, the build is refused and the tests run on the binaries already on disk, which don't include Live Coding patches.

## Structure

- `Fellowship.uproject` — module list and plugins (`ModelingToolsEditorMode`, editor-only).
- `Source/Fellowship.Target.cs`, `Source/FellowshipEditor.Target.cs` — build targets (BuildSettings V8, include order `Unreal5_9`). With V8, `Tests/` folders compile only into the editor target. New modules must be added to `ExtraModuleNames` here and to `Modules` in the `.uproject`.
- `Source/Fellowship/` subfolders (`Character/`, `Game/`, `Inventory/`, `Player/`) mirror `Content/Blueprints/`; put a C++ class in the folder matching its Blueprint subclass. The exceptions are `Animation/`, which holds animation code (anim instance, climbing IK, pose search setup) even though the Anim Blueprint lives in `Content/Blueprints/Character/`, `HUD/`, which holds the HUD and what it draws or places (equipment display, world label pool) even though `BP_FSHUD` lives in `Content/Blueprints/Player/`, and `Enemies/`, which holds the enemy, its definition, behaviors, spawner and registry even though their Blueprints live in `Content/Blueprints/Character/Enemies/`. The module root is on the include path (`Fellowship.Build.cs`), so cross-folder includes are written `#include "Character/FSCharacter.h"`.
- `Source/FellowshipEditor/` — editor-only module, for editor tooling such as the item asset's preview window and thumbnail.
- `Source/Fellowship/Fellowship.Build.cs` — module deps. Public deps already include `EnhancedInput`; Slate and OnlineSubsystem are present as commented-out lines.
- `Config/Default*.ini` — project settings. Rendering is configured for Lumen GI/reflections, hardware ray tracing, Virtual Shadow Maps, Substrate materials, DX12/SM6, no static lighting.
- `Content/` — binary `.uasset`/`.umap` files; edit through the editor, not as text.
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` — generated; ignore. Logs are in `Saved/Logs/`.
