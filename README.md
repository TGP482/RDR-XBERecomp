# Red Dead Revolver - XBE Recomp
An unofficial native PC port of the original Xbox version of Red Dead Revolver via static recompilation, built into a 64 bit Windows executable. The game's x86 code is translated to C++ ahead of time, the Xbox kernel, Direct3D and DirectSound calls are implemented on Windows, and everything is drawn with Direct3D 12 and played through XAudio2. Made with the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation
The latest version of [RDR-XBERecomp](https://github.com/TGP482/RDR-XBERecomp/releases) can be found in the Releases page.

- Place your game files in the `assets` folder. This can be an ISO, or the extracted data files.
- Run `RDR.exe`. The first start compiles the game's code for your PC, this only happens once.
- Recomp Settings can be adjusted in `settings.toml` beside the exe.

## Features
### Display
- **Display Mode** - Switch between Fullscreen, Borderless and Windowed.
- **Resolution** - Lists every mode your monitor reports and defaults to your desktop resolution, replacing the original resolution.
- **Render Resolution** - Renders the game at 20% to 200% of the output resolution, allowing supersampling or downscaling independent of the window size.
- **Max Frame Rate** - Caps the frame rate to your monitor's refresh rate or a custom value, or removes the cap entirely.
- **V-Sync** - Reduces screen tearing.

### Graphics
- **Anisotropic Filtering** - Forces the selected anisotropic filtering level on all textures.

### Textures
- **Texture Replacement** - Replaces the game's textures with the `.dds` or `.png` images in the `textures\replacement` folder beside the exe.
- **Texture Dumping** - Saves every texture the game uses into the `textures\dumps` folder beside the exe as you play.
- **Dump Format** - Choose between PNG or DDS, which keeps the original compression and mipmaps.

### Input
- **Controller** - Controller with SDL3 support for Xbox, PlayStation and Switch controllers, with hot plugging and rumble.

### Miscellaneous
- **Boot Splash** - Place a `splash.png` (or `.bmp`) beside the exe to display a splash screen at launch.

## System Requirements
### Minimum
- **OS**
  - Windows 10 64-bit
- **CPU** with 2 cores and support for SSE4.2 and POPCNT
  - Intel Nehalem (Intel Core 1st Generation)
  - AMD Bulldozer (AMD FX series)
- **GPU** with support for Direct3D 12
  - NVIDIA GeForce GT 630 (Kepler)
  - AMD Radeon HD 7750
  - Intel HD Graphics 510 (Skylake)
- **Memory**
  - 4 GB
- **Storage**
  - 4 GB for the game files, the compiled game code and saves

## Building
### 1. Clone the Repository
Clone this repository using [Git](https://git-scm.com/).
```
git clone https://github.com/TGP482/RDR-XBERecomp.git
```

### 2. Add the Required Game Files
Place your extracted game data files in the `assets` folder. `default.xbe` must be there, with the `.dat` files and the `bik`, `bik50`, `lvls_m2`, `lvls_m4` and `lvls_sng` folders beside it.

> [!NOTE]
> Building does not support the ISO format, so the files must be extracted. ISOs only work for playing.

### 3. Install Dependencies
Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) and select the following **Workloads** and **Individual components**:
- Desktop development with C++
- C++ Clang tools for Windows
- C++ CMake tools for Windows

> [!TIP]
> `build.bat` offers to install the Clang and CMake components for you if they're missing.

### 4. Build the Project
- Run `build.bat`. The first build recompiles the entire game and can take 30 to 90 minutes on weaker systems.
- When it finishes, run `out\build\release\RDR.exe`.
