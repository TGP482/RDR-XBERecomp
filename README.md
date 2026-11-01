# Red Dead Revolver - XBE Recomp
An unofficial native PC port of the original Xbox version of Red Dead Revolver, with extended graphics options, keyboard and mouse support, and other enhancements.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation
The latest version of [RDR-XBERecomp](https://github.com/TGP482/RDR-XBERecomp/releases) can be found in the Releases page.

- Put your game files in the `assets` folder. This can be an ISO, or the extracted data files.
- Run `RDR.exe`. The first start compiles the game's code for your PC, this only happens once.
- Recomp settings can be adjusted through the Settings overlay, which is opened with F10 by default.

## Features
### Display
- **Monitor** - Choose which monitor the game is displayed on.
- **Display Mode** - Switch between Fullscreen, Borderless and Windowed.
- **Resolution** - Lists every mode your monitor reports and defaults to your desktop resolution, replacing the original 640x480 resolution.
- **Render Resolution** - Renders the game at 20% to 200% of the output resolution, allowing supersampling or downscaling independent of the window size.
- **Field of View** - Sets the horizontal field of view during gameplay from 45 to 140 degrees.
- **Max Frame Rate** - Caps the frame rate to your monitor's refresh rate or a custom value, or removes the cap entirely.
- **V-Sync** - Reduces screen tearing.
- **HDR Output** - Displays the game in HDR10 on monitors with Windows HDR turned on, keeping white at your Windows SDR content brightness.
- **FPS Counter** - Displays the frame rate on screen.
- **FMV Scale** - Choose between Fill, which makes the FMVs fill widescreen displays, and 4:3.
- **Colour Blind Mode** - Adjusts the colours for protanopia, deuteranopia or tritanopia.

### Graphics
- **Ambient Occlusion** - Adds screen space ambient occlusion, creating soft contact shadows where surfaces meet. Choose between SSAO and GTAO.
- **Multisample Antialiasing (MSAA)** - Enables 2x, 4x or 8x MSAA to smooth jagged edges while preserving a sharp image. MSAA does not smooth alpha-tested edges such as fences and foliage.
- **Subpixel Morphological Antialiasing (SMAA)** - Enables SMAA to smooth jagged edges across the whole frame with a softer image. SMAA also smooths alpha-tested edges such as fences and foliage, and can be combined with MSAA.
- **Anisotropic Filtering** - Forces the selected anisotropic filtering level on all textures.
- **Screen Blur** - Toggles the screen blur the game applies during gameplay.

### Textures
- **Texture Replacement** - Replaces the game's textures with the .dds or .png images in the textures\replacement folder beside the exe.
- **Texture Dumping** - Saves every texture the game uses into the textures\dumps folder beside the exe.
- **Dump Format** - Choose between PNG or DDS, which keeps the original compression and mipmaps.

### Mouse and Keyboard
Full keyboard and mouse support with raw input, adjustable sensitivity, inverted look and rebindable controls.

### Controller
Controller support through SDL3 for Xbox, PlayStation and Switch controllers, with hot plugging and rumble.
- **Aim Sensitivity** - Adjusts the horizontal and vertical aim sensitivity beyond the in-game sliders.
- **Gyro Aiming** - Enables gyro aiming on controllers with a gyroscope.

### General
- **Skip Intro** - Skips the intro logos.
- **Pause On Focus Loss** - Pauses the game and its sound while the window is in the background.
- **Language** - Choose between English, French, German, Italian and Spanish, or Automatic, which follows the Windows display language. Applies after a restart.
- **Audio Output** - Choose between Automatic, Mono, 2.0 Stereo and 5.1 Surround.
- **Toggle HUD** - Hides the HUD with a bindable key.

### Miscellaneous
- **Boot Splash** - Place a `splash.png` (or `.bmp`) beside the exe to display a splash screen at launch.
- **Custom Menu Cursor** - Place a `cursor.png` (or `cursor.dds`) beside the exe to replace the Windows arrow in the game's menus.

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
