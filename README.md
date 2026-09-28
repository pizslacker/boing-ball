# Amiga Boing Ball

A complete, self-contained C program using SDL2 that replicates the famous 1985 Amiga Boing Ball demo.

The original demo relied on hardware features like color cycling (shifting the palette to simulate rotation) and the blitter for movement. Because modern hardware works differently, this replication uses real-time software raycasting to draw and map the checkered pattern onto a mathematically tilted 3D sphere. It accurately recreates the exact perspective grid (See below), smooth physics, the distinctive 15-degree right tilt, and the translucent shadow.

### Perspective grid:

To achieve this exact look, I applied a classic graphics trick called the Painter's Algorithm:

1. We establish a vanishing point for the floor higher up on the screen.

2. We calculate the exact mathematical intersection where the outermost radiating floor lines hit the left and right edges of the window (X=0 and X=WIDTH).

3. We set our horizon line exactly at that Y-coordinate.

4. We draw the floor lines first, then lift the background layer over the floor by rendering a solid grey rectangle from the top of the screen down to the horizon, physically painting over the floor's vanishing point.

5. Finally, we draw the back wall grid over that solid layer.
