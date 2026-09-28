#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Screen and Ball constraints
#define WIDTH 1280
#define HEIGHT 1024
#define R 100
#define LON_SEGMENTS 16
#define LAT_SEGMENTS 8
#define FPS 60

// Physics constants
#define GRAVITY 0.3f
#define BOUNCE_DAMPENING 1.0f // 1.0 = endless bounce (like the original)

// Helper to constrain values
float clamp(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// Draw the classic Amiga perspective grid
void draw_background_grid(SDL_Renderer* renderer) {
    // Fill background with light grey
    SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
    SDL_RenderClear(renderer);

    // Set grid color to purple/magenta
    SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);

    int horizon = HEIGHT / 2;

    // --- Back Wall Grid ---
    for (int x = 0; x <= WIDTH; x += 50) {
        SDL_RenderDrawLine(renderer, x, 0, x, horizon);
    }
    for (int y = 0; y <= horizon; y += 50) {
        SDL_RenderDrawLine(renderer, 0, y, WIDTH, y);
    }

    // --- Floor Perspective Grid ---
    // Converging lines
    for (int i = -20; i <= 20; i++) {
        int end_x = WIDTH / 2 + i * 150; 
        SDL_RenderDrawLine(renderer, WIDTH / 2, horizon, end_x, HEIGHT);
    }
    
    // Horizontal perspective lines
    float cur_y = horizon;
    float step = 2.0f;
    while (cur_y < HEIGHT) {
        SDL_RenderDrawLine(renderer, 0, (int)cur_y, WIDTH, (int)cur_y);
        cur_y += step;
        step *= 1.15f; 
    }
}

// Procedurally generate the texture of the tilted, rotated sphere
void update_ball_texture(SDL_Texture* tex, float phase) {
    void* pixels;
    int pitch;
    SDL_LockTexture(tex, NULL, &pixels, &pitch);
    Uint32* dst = (Uint32*)pixels;

    // The Amiga boing ball is tilted about 15 degrees to the right
    float tilt = 15.0f * M_PI / 180.0f;
    float cos_tilt = cos(-tilt);
    float sin_tilt = sin(-tilt);
    
    float cos_phase = cos(-phase);
    float sin_phase = sin(-phase);

    for (int y = 0; y < R * 2; y++) {
        for (int x = 0; x < R * 2; x++) {
            float px = x - R;
            float py = y - R;
            float r2 = px * px + py * py;
            
            // If inside the radius of the sphere
            if (r2 <= R * R) {
                float pz = sqrt(R * R - r2);

                // Apply inverse tilt around Z axis
                float x1 = px * cos_tilt - py * sin_tilt;
                float y1 = px * sin_tilt + py * cos_tilt;
                float z1 = pz;

                // Apply inverse spin around Y axis
                float x0 = x1 * cos_phase + z1 * sin_phase;
                float y0 = y1;
                float z0 = -x1 * sin_phase + z1 * cos_phase;

                // Convert to Latitude / Longitude
                float y_norm = clamp(y0 / R, -1.0f, 1.0f);
                float lat = asin(y_norm);
                float lon = atan2(x0, z0);

                // Map to checkered segments
                int lon_idx = (int)floor((lon + M_PI) / (2 * M_PI) * LON_SEGMENTS);
                int lat_idx = (int)floor((lat + M_PI / 2) / M_PI * LAT_SEGMENTS);

                // Determine color
                bool is_red = (lon_idx + lat_idx) % 2 == 0;
                
                // ARGB8888 Pixel Format
                dst[y * (pitch / 4) + x] = is_red ? 0xFFFF0000 : 0xFFFFFFFF; 
            } else {
                // Transparent background outside the sphere
                dst[y * (pitch / 4) + x] = 0x00000000; 
            }
        }
    }
    SDL_UnlockTexture(tex);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Amiga Boing Ball 1985", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Ball texture (updated every frame for rotation)
    SDL_Texture* ball_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                              SDL_TEXTUREACCESS_STREAMING, R * 2, R * 2);
    SDL_SetTextureBlendMode(ball_tex, SDL_BLENDMODE_BLEND);

    // Static shadow texture (a simple translucent dark circle)
    SDL_Texture* shadow_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                                SDL_TEXTUREACCESS_STATIC, R * 2, R * 2);
    SDL_SetTextureBlendMode(shadow_tex, SDL_BLENDMODE_BLEND);
    
    // Generate the shadow once
    Uint32* shadow_pixels = (Uint32*)malloc(4 * R * 2 * R * 2);
    for (int y = 0; y < R * 2; y++) {
        for (int x = 0; x < R * 2; x++) {
            float px = x - R;
            float py = y - R;
            // 0x55000000 is a transparent black
            shadow_pixels[y * (R * 2) + x] = (px * px + py * py <= R * R) ? 0x55000000 : 0x00000000;
        }
    }
    SDL_UpdateTexture(shadow_tex, NULL, shadow_pixels, R * 2 * 4);
    free(shadow_pixels);

    // Physics State
    float ball_x = WIDTH / 2.0f;
    float ball_y = HEIGHT / 4.0f;
    float vel_x = 4.0f;
    float vel_y = 0.0f;
    float phase = 0.0f;
    float rot_vel = 0.06f;

    bool quit = false;
    SDL_Event e;
    Uint32 frame_delay = 1000 / FPS;

    while (!quit) {
        Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                quit = true;
            }
        }

        // --- Physics Update ---
        vel_y += GRAVITY;
        ball_x += vel_x;
        ball_y += vel_y;
        phase += rot_vel;

        // Bounce off side walls
        if (ball_x + R > WIDTH) {
            ball_x = WIDTH - R;
            vel_x = -vel_x;
            rot_vel = -rot_vel; // Reversing spin direction creates a nice physical reaction
        } else if (ball_x - R < 0) {
            ball_x = R;
            vel_x = -vel_x;
            rot_vel = -rot_vel;
        }

        // Bounce off floor
        if (ball_y + R > HEIGHT) {
            ball_y = HEIGHT - R;
            vel_y = -vel_y * BOUNCE_DAMPENING;
        }

        // --- Rendering ---
        draw_background_grid(renderer);

        // Update the raycasted 3D sphere texture
        update_ball_texture(ball_tex, phase);

        // Draw Shadow (offset slightly to the bottom right)
        SDL_Rect shadow_rect = { (int)ball_x - R + 30, (int)ball_y - R + 20, R * 2, R * 2 };
        SDL_RenderCopy(renderer, shadow_tex, NULL, &shadow_rect);

        // Draw Ball
        SDL_Rect ball_rect = { (int)ball_x - R, (int)ball_y - R, R * 2, R * 2 };
        SDL_RenderCopy(renderer, ball_tex, NULL, &ball_rect);

        SDL_RenderPresent(renderer);

        // Cap framerate
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_delay > frame_time) {
            SDL_Delay(frame_delay - frame_time);
        }
    }

    // Cleanup
    SDL_DestroyTexture(ball_tex);
    SDL_DestroyTexture(shadow_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}