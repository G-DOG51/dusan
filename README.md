# Apex DMA — Windows KM Driver

Windows-native Apex Legends DMA tool using a kernel-mode driver for memory access, with a DirectX 11 + ImGui in-game overlay.

> **Forked from** [chettoy/apex_dma_kvm_pub](https://github.com/chettoy/apex_dma_kvm_pub) — converted from Linux/KVM + memflow + Rust to a pure Windows C++ project with KM driver.

---

## Features

### Aimbot
- Multiple aim modes (Closest to Crosshair / Closest Distance)
- Configurable ADS and non-ADS FOV
- Adjustable smoothing (primary & secondary hotkey)
- Auto bone selection and nearest bone targeting
- Headshot distance threshold
- Recoil control (pitch & yaw compensation)
- Visibility check toggle
- Sky grenade auto-aim
- Trigger bot with per-weapon fire delay
- Flick bot for single-shot weapons

### Glow / ESP
- Player glow with visibility-based colors (visible / not visible / knocked)
- Armor-based color coding (white → blue → purple → red → gold)
- Configurable inside value, outline size, and glow distance
- Weapon model glow indicating spectator count
- Rainbow glow for favorite players
- Item glow with full loot filter (backpacks, shields, helmets, mags, stocks, barrels, optics, hop-ups, ammo, grenades, weapons)
- Death box highlighting

### Overlay (DirectX 11 + ImGui)
- Transparent borderless overlay rendered on top of the game window
- 6-tab settings menu: **Aimbot**, **Visuals**, **Glow**, **Loot Filter**, **Prediction**, **Misc**
- Toggle with **INSERT** key
- Click-through when menu is hidden; interactive when menu is shown
- Spectator count display in status bar

### Movement
- Auto tap-strafe
- Auto super-glide (supports 75 / 144 / 240 FPS)
- Super grapple (auto jump on grapple attach)

### Misc
- Per-weapon projectile prediction tuning
- Map radar
- Firing range dummy targeting
- TDM team detection
- Game FPS calculation
- Save / load settings via config file

---

## Requirements

- **Windows 10/11** (x64)
- **Visual Studio 2022+** with C++ Desktop Development workload (MSVC v143+ toolset)
- **KM driver** loaded and running (the tool communicates via `\\.\eSIGNuTILITES1`)
- **Apex Legends** (Steam, running on the same machine)

---

## Build

1. Open `apex_dma.sln` in Visual Studio.
2. Select **Release | x64**.
3. Build the solution (**Ctrl+Shift+B**).
4. Output: `x64\Release\apex_dma.exe`

No CMake, no Rust, no external package manager required.

---

## Usage

1. Load the KM driver.
2. Launch **Apex Legends**.
3. Run `apex_dma.exe`.
   - The console will display `Searching for apex process...` then `Apex process found` once detected.
   - The overlay will print `Waiting for game window...` and attach once the game window is found.
4. Press **INSERT** to toggle the settings menu on/off.
5. Configure settings in the overlay tabs or use the terminal menu.

### Hotkeys (defaults)

| Key | Action |
|-----|--------|
| INSERT | Toggle overlay menu |
| F3 | Quick toggle aimbot on/off |
| F4 | Quick toggle glow on/off |
| F8 | Map radar pulse |

---

## Update Offsets

```shell
py update.py offsets.h offsets.ini
```

---

## Project Structure

```
apex_dma.sln
apex_dma/
├── apex_dma.vcxproj         # VS project (x64 Release, MSVC v143, C++17)
├── apex_dma.cpp              # Main entry, thread management
├── Game.h / Game.cpp         # Entity, weapon, player classes
├── GameMath.h / Math.cpp     # Vector math, world-to-screen, aim calculation
├── memory.hpp / memory.cpp   # KM driver memory read/write wrapper
├── apex_sky.h                # Settings structs, function declarations
├── apex_sky_stubs.cpp        # C++ stubs (replaced Rust FFI)
├── offsets.h                 # Game offsets
├── items.h                   # Item ID enums
├── vector.h                  # Vector/Matrix types
├── km/
│   └── km_driver.h           # Kernel-mode driver communication (header-only)
└── Client/
    ├── overlay_dx11.cpp       # DX11 + ImGui overlay & settings menu
    ├── overlay_dx11.h
    ├── main.h
    └── imgui/                 # ImGui v1.91.8 (source included)
```

---

## Credits

- [chettoy/apex_dma_kvm_pub](https://github.com/chettoy/apex_dma_kvm_pub) — original Linux/KVM fork
- [Jotalz/apex_dma_kvm_pub](https://github.com/Jotalz/apex_dma_kvm_pub) — upstream repository
- [KrackerCo/apex_dma_kvm_pub](https://github.com/KrackerCo/apex_dma_kvm_pub) — no-overlay and client branches
- [MisterY52/apex_dma_kvm_pub](https://github.com/MisterY52/apex_dma_kvm_pub) — original project
- [CasualX/apexdream](https://github.com/CasualX/apexdream) — sky grenade math
- [Dear ImGui](https://github.com/ocornut/imgui) — GUI framework
- [memflow](https://github.com/memflow/memflow) — original memory access library (replaced with KM driver)
