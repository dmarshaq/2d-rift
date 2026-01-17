# :video_game: Rift Game

**Is a 2D demo of a puzzle game fully built in C with only dependencies being OpenGL, SDL2 and other smaller C libraries.**

![License](https://img.shields.io/badge/License-MIT-blue)
![License](https://img.shields.io/badge/Platform-Windows-blue)
![C](https://img.shields.io/badge/C-blue)
![OpenGL](https://img.shields.io/badge/OpenGL-blue)
![SDL2](https://img.shields.io/badge/SDL2-blue)



<p align="center">
  <img src="https://github.com/user-attachments/assets/be8de21d-795a-4b88-a835-4099c45231d2" width="600" />
  <br />
  <em>In game level demo</em>
</p>



:pushpin: Table Of Contents
-----------------
* [Quick Start](#bulb-quick-start)
    * [Requirements](#clipboard-requirements)
    * [Build](#hammer-build)
    * [How to Run](#tada-how-to-run)
* [Features](#art-features)
* [About](#dart-about)
* [License](#page_facing_up-license)
* [Collaboration](#seedling-collaboration)

:bulb: Quick Start
-----------------

Since game is built and can be launched on Windows only, to install and use needed libraries and GNU tools use MinGW environment. Which is a free, open-source software development environment that provides a port of the GNU Compiler Collection (GCC) and related tools on Windows.

#### :clipboard: Requirements 
* GCC or Clang (C compiler)
* Git
* SDL2 library
* OpenGL development headers

#### :hammer: Build 
To build the project:

1. Open a terminal and navigate to your project directory.
2. Clone the repository:
   ```
   git clone https://github.com/dmarshaq/game-in-c.git
   ```
3. Build using the NoBuild:
   ```
   gcc -o nob nob.c
   ./nob
   ```

#### :tada: How to Run
- After building, launch the game using the compiled executable through terminal from project directory.

   ```
   ./bin/game.exe
   ```

:art: Features
-----------------
- Custom project build system using a meta-programming preprocessor and NoBuild tool.
- Dynamic asset manager allowing real-time loading and modification of game resources.
- Modular C codebase designed for scalability and reuse in future game projects.
- Low-level OpenGL graphics rendering for 2D interface and gameplay visuals.
- Focus on learning, experimentation, and building a robust foundation for game development.


<p align="center">
  <img alt="{307D7308-B992-4DE5-9FAE-51AC5A1DF03E}" src="https://github.com/user-attachments/assets/ad40689d-ed63-4375-afc4-758e2eb50f9d" width="700" />
  <br />
  <em>In game level editor</em>
</p>

:dart: About
-----------------
This project is both a 2D game and a framework for creating future games in C. It explores low-level systems programming, graphics programming with OpenGL, and asset management. The goal is to provide a flexible, reusable codebase while delivering a functional game that showcases core engine capabilities.

:page_facing_up: License
-----------------
The MIT License (MIT)

Copyright (c) 2025 Daniil Yarmalovich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

:seedling: Collaboration
-----------------
Please open an issue in the repository for any questions or problems.
Alternatively, you can contact me at mrshovdaniil@gmail.com.
