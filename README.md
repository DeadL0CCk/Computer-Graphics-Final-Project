# CityLifeFinalFile

A single-file C++ OpenGL demo that presents four animated city and landscape scenes in one program. Each scenario is built into its own namespace, allowing the app to switch between them smoothly without name collisions.

## Overview

CityLifeFinalFile.cpp creates a self-contained interactive scene viewer with these four environments:

- Dynamic Coastal City
  - day and night lighting
  - storm and weather variation
  - lighthouse and helicopter motion

- Downtown Neon District
  - rainy nighttime city mood
  - glowing signage and wet street reflections
  - elevated train atmosphere

- Riverside Park
  - cherry blossoms and seasonal color shifts
  - ferris wheel, kayaks, and scenic shoreline details
  - multiple time-of-day phases

- Winter Night Market
  - snowy plaza and festive winter ambience
  - river, traffic, and seasonal lighting changes
  - snow squalls and different lighting palettes

The application opens directly on Scenario 1 and lets the user move between scenes using keyboard controls.

## Controls

### Scene navigation

- N or Right Arrow: next scene
- B or Left Arrow: previous scene
- F1-F4: jump directly to a specific scene
- ESC: quit the program

### Global controls

- Space: pause / resume
- H: show / hide the control panel
- X: trigger this scenario's rare event or demo helper

### Scenario-specific controls

- Scenario 1 (Coastal City)
  - 1: day
  - 2: night
  - 3: cycle weather

- Scenario 2 (Downtown)
  - 1: dusk
  - 2: night
  - 3: dawn
  - R: rain
  - T: storm

- Scenario 3 (Riverside Park)
  - 1-4: morning, midday, golden hour, dusk
  - W: wind level
  - A: blossom / autumn color mode

- Scenario 4 (Winter Night Market)
  - 1-3: snow amount
  - 4 or 5: day or night
  - S: season
  - L: light color
  - F: snow squall

## Prerequisites

This project is written for Windows and uses OpenGL + GLUT.

Required tools and libraries:

- Windows operating system
- C++ compiler with C++11 support
- OpenGL library
- FreeGLUT library
- GLU library

Typical toolchains include:

- MinGW with g++
- Code::Blocks with the GNU GCC compiler

## Build instructions

### Option 1: Code::Blocks

1. Open the project in Code::Blocks.
2. Ensure the compiler is configured to use the C++11 standard.
3. In the compiler settings, enable the flag:

   -std=gnu++11

   or use the equivalent option in the GUI for "Have g++ follow the C++11 ISO C++ language standard".
4. Build the project.

### Option 2: Command line (MinGW / g++)

From the project directory, run:

```bash
g++ -std=gnu++11 CityLifeFinalFile.cpp -o CityLife -lfreeglut -lopengl32 -lglu32
```

If you are using a Windows environment with FreeGLUT correctly installed and configured, this should produce a runnable binary.

## Run the program

After building, execute the output binary:

```bash
CityLife.exe
```

or, on a Unix-like shell environment if the binary is built there:

```bash
./CityLife
```

Once launched, the app starts in the coastal city scene and you can browse through the other scenarios using the keys above.

## Screenshots

[Placeholder: Add screenshots of the four scenes here]

## Notes

- This project is intentionally a single-file demo and does not include a menu screen.
- The C++ source file contains detailed comments explaining scene structure, rendering helpers, and the required C++11 build setting.
- If the program fails to compile with errors mentioning `constexpr`, `nullptr`, or missing macros, the compiler is likely not using the C++11 flag.

## Project file

- `CityLifeFinalFile.cpp` - main OpenGL source file for all scenes and controls
