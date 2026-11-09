# Fellowship

A third-person action RPG prototype built in **Unreal Engine 5.9** (C++). Fellowship is organized around four pillars. Each one is a group of systems the prototype exists to prove out:

| Pillar | Scope | Status |
| --- | --- | --- |
| [1. Metasystems](#1-metasystems) | Inventory, loot, character customization | Inventory well along; loot and customization early |
| [2. Animation Systems](#2-animation-systems) | Climbing, motion matching, procedural animation | Working foundations for all three |
| [3. Gameplay](#3-gameplay) | Combat, questing, AI | Combat foundations and the enemy framework are in; questing not started |
| [4. Multiplayer](#4-multiplayer) | Client-server, persistence, cloud containers | Planned; the game is single-player for now |

Legend: ✅ working · 🚧 in progress / partial · 📋 planned

---

## 1. Metasystems

What the player collects, carries and becomes across a session.

### Inventory ✅
A spatial, grid-based inventory that lives inside the backpack you wear.

- **Backpack grids.** The backpack you wear sets the grid size (`UFSItemDefinition::InventoryDimensions`). Items take up multiple cells and rotate in quarter turns. Auto-placement tries upright first and lies items on their side if needed (`Inventory/FSInventoryGrid`).
- **Hands, quick slots and armor.** Two hand slots, two quick slots that hang off the sides of the backpack mesh, and Head/Face/Body armor slots (`Inventory/FSInventoryComponent`).
- **Dropped backpacks keep their contents.** A backpack on the ground still holds everything inside it, and its quick-slot items still hang off it.
- **Diegetic inventory screen.** Opening the inventory switches to a first-person rig: you hold the swaying backpack up, the grid floats in front of it, your forearms are raised behind the hand slots, and nearby loot is listed alongside. You can keep walking slowly with it open (`Inventory/FSInventoryRig`).
- **Cursor handling.** Click to pick up, swap or place items, double-click for quick moves, and click outside to discard (`Player/FSPlayerController`).
- **Equipment display.** The real meshes of what you have equipped float in HUD circles and shake as you move (`HUD/FSEquipmentDisplay`).
- **Item authoring.** Making a new item takes one `UFSItemDefinition` data asset. The `FellowshipEditor` module opens it in a custom editor with a live 3D preview, and the Content Browser thumbnail renders its mesh.

### Loot 🚧
- ✅ World pickups for any item (`Inventory/FSItemPickup`), with floating labels coloured by rarity (Common → Legendary).
- ✅ Item types: consumables (potions), weapons (sword, two-handed sword, spear, bow, wand, shield), backpacks and armor.
- ✅ Event log of pickups and other happenings (`Game/FSEventLog`).
- 📋 Loot tables, enemy drops, containers and randomized rolls.

### Character Customization 📋
- 🚧 In-world **Character Creator** and **Stats Editor** interaction volumes (`Game/FSInteractionVolume`). The entry points are wired up, but the editors aren't built yet.
- ✅ Health, magic and stamina stats (`Character/FSStatsComponent`).
- ✅ Worn armor and backpacks show on the character (`Character/FSEquipmentComponent`).
- 📋 Appearance editing (body, face, colours) and stat allocation.

---

## 2. Animation Systems

How the character moves and how the body responds to the world.

### Climbing ✅
- A custom `Climbing` movement mode (`Character/FSCharacterMovementComponent`) that follows walls, overhangs and slopes, and handles getting over ledges.
- The anim instance gets climb blend-space inputs, climb speed and surface lean (`Animation/FSCharacterAnimInstance`).

### Motion Matching ✅
- Locomotion uses **Pose Search** with a trajectory fed from the movement component, plus debug drawing of the query and the selected pose.
- **Weapon stances** (`Inventory/FSWeaponStance`): each combination of held weapons picks its own motion-matching database, so a sword-and-shield moves differently from a spear.
- Editor helper (`Animation/FSPoseSearchSetupLibrary`) that lets Python setup scripts edit database entries.

### Procedural Animation ✅
- **Climbing IK** (`Animation/FSClimbIKComponent`): hands and feet trace for the surface and plant on it through IK Rig. Diagonal limb pairs step in a climbing gait, and the hands reach for the top of a ledge when climbing over.
- The spine bends toward the hands while getting over a ledge.
- Secondary motion: backpack sway, dangling quick-slot items, camera bob in the inventory, and shake on the equipment display.

---

## 3. Gameplay

What the player does moment to moment.

### Combat 🚧
- ✅ Draw and sheathe weapons, with per-hand equip animations (`Character/FSEquipmentComponent`).
- ✅ Per-slot **use input** for each hand and quick slot, lit up on the HUD (`Character/FSItemUseComponent`).
- ✅ **Targeting / lock-on**: the camera swings behind the character, the arm zooms in, the character strafes, and cinematic bars slide in (`Character/FSCharacter`, `HUD/FSHUD`).
- ✅ Stats and damage handling. Enemies have health bars, hit reactions, death animations and corpse cleanup.
- 📋 Melee hit detection, blocking, ranged and magic attacks, and stamina costs.

### AI 🚧
- ✅ Data-driven enemies. An `UFSEnemyDefinition` asset sets the mesh, size, stats, speed and animations, plus a pluggable `UFSEnemyBehavior` class that defines how the enemy moves, thinks and attacks (`Enemies/`).
- ✅ Enemy spawners with respawn, and a world registry the HUD uses to find enemies.
- 🚧 First enemy, **the Prick** (a walking cactus). Its assets are in, but its behavior is a stub.
- 📋 Perception, navigation, attack patterns and group behavior.

### Questing 📋
- 📋 Quest definitions, objectives, tracking UI and rewards. The interactable framework (`Game/FSInteractable`, `Game/FSInteractableRegistry`) and the event log are the starting point.

---

## 4. Multiplayer

Fellowship is meant to be a co-op game. For now, gameplay is deliberately built **single-player first**, with a planned refactor for networking. The world never pauses, even with the inventory open, so co-op will feel the same.

### Client-Server 📋
- Dedicated or listen server authority over the inventory, combat and enemies, with replicated movement covering the custom climbing mode and motion-matching trajectories.
- The existing split is already shaped for this: `FSGameMode`, `FSGameState`, `FSPlayerState` and `FSPlayerController`, plus local-only presentation such as the inventory rig, which exists only on the owning client.

### Persistence 📋
- Save and load for the character, inventory (including dropped backpacks' contents) and world state such as quest progress.

### Cloud Containers 📋
- Containerized dedicated servers for hosted sessions, with matchmaking or session discovery through an online subsystem.

---

## Project Layout

```
Source/
  Fellowship/            Runtime module
    Animation/           Anim instance, climbing IK, pose search setup
    Character/           Character, movement, equipment, item use, stats
    Enemies/             Enemy, definitions, behaviors, spawner, registry
    Game/                Game mode/state, interactables, event log
    HUD/                 HUD, equipment display, world label pool
    Inventory/           Inventory component, grid, rig, items, pickups, stances
    Player/              Player controller and state
    Tests/               Automation tests
  FellowshipEditor/      Editor-only tooling (item editor, preview, thumbnails)
Content/                 Assets (Git LFS, lockable)
Config/                  Project settings
Scripts/                 Build helpers
```

C++ folders mirror `Content/Blueprints/`. The exceptions are `Animation/`, `HUD/` and `Enemies/` (see [CLAUDE.md](CLAUDE.md)).

## Building

Requires Unreal Engine 5.9 (launcher build at `C:\Program Files\Epic Games\UE_5.9`), Visual Studio with MSVC (C++20) and Git LFS.

```powershell
# Build the editor target
& "C:\Program Files\Epic Games\UE_5.9\Engine\Build\BatchFiles\Build.bat" FellowshipEditor Win64 Development "C:\dev\Fellowship\Fellowship.uproject" -waitmutex

# Or close the editor, build, and reopen it
.\Scripts\BuildAndRelaunch.ps1
```

If the editor is open with Live Coding enabled, compile from the editor instead (Ctrl+Alt+F11).

## Testing

Automation tests in `Source/Fellowship/Tests/` cover climbing, enemies, equipment, the event log, interaction volumes, the inventory and its rig, item use, stats and targeting. Running `Fellowship` runs all of them:

```powershell
UnrealEditor-Cmd.exe "C:\dev\Fellowship\Fellowship.uproject" -ExecCmds="Automation RunTests Fellowship;Quit" -unattended -nullrhi -nopause
```

## Rendering

Lumen GI and reflections, hardware ray tracing, Virtual Shadow Maps, Substrate materials and DX12/SM6, with no static lighting.
