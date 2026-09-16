# True 3D Amiga Boing Ball (C/SDL2/OOP)

A lightweight, object-oriented programmed demoscene engine written in pure C, utilizing SDL2. 

This project demonstrates how to build a flexible, polymorphic rendering engine in C without relying on C++. It recreates the classic 1984 Amiga boing ball demo, complete with a falling OS curtain and a pair of procedurally generated "Xeyes" that track the ball in real-time.

## Features

* **True 3D Software Rasterization:** The Amiga boing ball is not a static 2D image. It is mathematically rasterized pixel-by-pixel onto a streaming texture every frame, complete with spherical perspective, latitude/longitude checkerboarding, and directional shading.
* **Classic Amiga Drop Shadow:** Recreated using hardware texture tinting and alpha blending.
* **X-Eyes Tracking:** Procedurally generated Xeyes that use 2D vector math to track the ball's coordinates in real-time, clamping pupils to an elliptical boundary.
* **Curtain Drop Transition:** A delayed gravity-simulation transition that drops a foreground image to reveal the demo underneath.
* **Audio Integration:** Dependency-injected `SDL_mixer` support for tracker modules (`.mod`) and collision sound effects.

## The Architecture: OOP in C

The engine is built around a custom Object-Oriented architecture using **Virtual Tables (vtables)**. 

Instead of a massive `switch` statement in the main loop, every visual effect inherits from a base `Effect` struct. The engine blindly iterates through a playlist of `Effect` pointers, calling their specific `update()`, `render()`, and `destroy()` functions via function pointers.

```c
// The core of the polymorphic engine
for (int i = 0; i < PLAYLIST_SIZE; i++) {
    if (playlist[i]->is_active) {
        playlist[i]->vptr->update(playlist[i], delta_time);
        playlist[i]->vptr->render(playlist[i], renderer);
    }
}
```

This allows new effects to be dropped into the engine with zero modifications to the main application loop. It also allows for Dependency Injection—for example, the XEyes constructor takes a pointer to the AmigaBoingBall so it can read its coordinates.

## Dependencies
To build and run this project, you need the SDL2 development headers:
- sdl2
- sdl2_image
- sdl2_mixer

On Debian/Ubuntu-based systems:
```bash
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev
```

## Required Assets
Place the following files in the root directory before running:

`workbench.png` (or `.jpg`) - The foreground image that falls away.

`bgm.mod` - A tracker module file for background music.

`boing.wav` - The sound effect triggered when the ball hits a wall.

## Build and Run
The project uses pkg-config in the Makefile to automatically link the correct SDL2 libraries for your system.
```bash
# Compile and execute the demo
make run

# Clean build artifacts
make clean
```
