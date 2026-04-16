# PortProject

## Project Summary
PortProject is a single-file OpenGL and GLUT scene written in C++. The environment includes:

- Sea with animated wave surface
- Lighthouse with rotating spotlight
- Multiple regular boats that float and bob on waves
- One larger container cargo ship with slower, heavier movement
- Flocking birds with style presets
- Keyboard-driven camera pan and zoom
- Debug toggles and FPS printouts

The scene is designed to stay simple, readable, and easy to build across different operating systems.

## Why There Is an If Condition in the Includes
At the top of main.cpp, there is a platform-specific include block:

- If compiling on Apple systems, it includes headers from the macOS framework paths
- Otherwise, it includes generic OpenGL and GLUT headers used by Linux and many Windows toolchains

This is needed because header locations differ by platform. On macOS, OpenGL and GLUT are provided as frameworks with different include paths than common non-macOS setups.

## Build and Run

### macOS
From the project folder:

```bash
g++ -DGL_SILENCE_DEPRECATION main.cpp -o port -framework OpenGL -framework GLUT
./port
```

### Linux (freeglut)
Install OpenGL and freeglut development packages, then:

```bash
g++ main.cpp -o port -lGL -lGLU -lglut
./port
```

### Windows (MinGW/freeglut)
Make sure freeglut and OpenGL libraries are available to your compiler, then:

```bash
g++ main.cpp -o port -lfreeglut -lopengl32 -lglu32
port.exe
```

## Runtime Controls

### Scene and Camera
- ESC or Q: quit
- + or =: zoom in
- - or _: zoom out
- I: pan camera up
- K: pan camera down
- J: pan camera left
- L: pan camera right

### Bird Flocking Style
- 1: tight flock
- 2: loose flock
- 3: faster seagull-like flock

### Section Toggles
- G: toggle ground
- W: toggle water
- H: toggle lighthouse
- O: toggle boats
- B: toggle birds
- P: toggle FPS and debug print

### Cargo Ship Control
- M: toggle cargo ship movement on or off

## Notes
- Optional sea texture support is included. If a sea.bmp file is present in the project folder, it can be loaded for water rendering.
- The project currently uses one main.cpp file to keep collaboration and debugging straightforward.
