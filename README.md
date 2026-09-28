# Amiga Boing Ball

A complete, self-contained C program using SDL2 that replicates the famous 1985 Amiga Boing Ball demo.

The original demo relied on hardware features like color cycling (shifting the palette to simulate rotation) and the blitter for movement. Because modern hardware works differently, this replication uses real-time software raycasting to draw and map the checkered pattern onto a mathematically tilted 3D sphere. It accurately recreates the exact perspective grid, smooth physics, the distinctive 15-degree right tilt, and the translucent shadow.
