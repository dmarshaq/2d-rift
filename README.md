# Rift Game

A 2D structured game built in C, designed as both a standalone project and a foundation for a reusable game development codebase.

Table Of Contents
-----------------
* [Quick Start](#quick-start)
    * [Requirements](#requirements)
    * [Build](#build)
    * [How to Run](#how-to-run)
* [Features](#features)
* [About](#about)
* [License](#license)
* [Collaboration](#collaboration)

Quick Start
-----------------
#### Requirements
* GCC or Clang (C compiler)
* Git
* SDL2 library
* OpenGL development headers

#### Build
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
4. Run the executable:
   ```
   ./bin/game
   ```

#### How to Run
- After building, launch the game using the compiled executable.

Features
-----------------
- Custom project build system using a meta-programming preprocessor and NoBuild tool.
- Dynamic asset manager allowing real-time loading and modification of game resources.
- Modular C codebase designed for scalability and reuse in future game projects.
- Low-level OpenGL graphics rendering for 2D interface and gameplay visuals.
- Focus on learning, experimentation, and building a robust foundation for game development.

About
-----------------
This project is both a 2D game and a framework for creating future games in C. It explores low-level systems programming, graphics programming with OpenGL, and asset management. The goal is to provide a flexible, reusable codebase while delivering a functional game that showcases core engine capabilities.

License
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

Collaboration
-----------------
Please open an issue in the repository for any questions or problems.
Alternatively, you can contact me at tewindale@gmail.com.
