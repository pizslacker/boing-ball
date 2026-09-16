#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 1024
#define PI 3.14159265358979323846

/* =========================================
 * 1. THE BASE CLASS (Effect)
 * ========================================= */

typedef struct Effect Effect;

typedef struct {
    void (*update)(Effect* self, float delta_time);
    void (*render)(Effect* self, SDL_Renderer* renderer);
    void (*destroy)(Effect* self);
} EffectVTable;

struct Effect {
    const EffectVTable* vptr;
    bool is_active;
};

/* =========================================
 * 2. DERIVED CLASS A (AmigaBoingBall)
 * ========================================= */

typedef struct {
    Effect base;
    float x, y;
    float vx, vy;
    int radius;
    Mix_Chunk* hit_sound;
    
    // For software 3D rendering
    SDL_Texture* streaming_texture; 
    float spin_angle; // How far it has rotated around its axis
    float tilt_angle; // The permanent slant (e.g., 15 degrees)
} AmigaBoingBall;

static void ball_update(Effect* self, float dt) {
    AmigaBoingBall* ball = (AmigaBoingBall*)self;

    // 1. Move Physics
    ball->x += ball->vx * dt;
    ball->y += ball->vy * dt;
    ball->spin_angle += 3.0f * dt; // Rotate the texture

    // 2. Collisions
    bool hit_wall = false;
    if (ball->x <= ball->radius) { ball->x = ball->radius; ball->vx *= -1; hit_wall = true; }
    else if (ball->x >= WINDOW_WIDTH - ball->radius) { ball->x = WINDOW_WIDTH - ball->radius; ball->vx *= -1; hit_wall = true; }
    
    if (ball->y <= ball->radius) { ball->y = ball->radius; ball->vy *= -1; hit_wall = true; }
    else if (ball->y >= WINDOW_HEIGHT - ball->radius) { ball->y = WINDOW_HEIGHT - ball->radius; ball->vy *= -1; hit_wall = true; }

    if (hit_wall && ball->hit_sound != NULL) Mix_PlayChannel(-1, ball->hit_sound, 0);

    // 3. Software 3D Sphere Rasterization
    void* pixels;
    int pitch;
    
    // Lock the GPU texture so the CPU can write to it directly
    SDL_LockTexture(ball->streaming_texture, NULL, &pixels, &pitch);
    Uint32* dst = (Uint32*)pixels;

    int R = ball->radius;
    int R2 = R * R;
    
    // Precalculate rotation math for this frame
    float cos_tilt = cos(-ball->tilt_angle);
    float sin_tilt = sin(-ball->tilt_angle);
    float cos_spin = cos(-ball->spin_angle);
    float sin_spin = sin(-ball->spin_angle);

    for (int py = 0; py < R * 2; py++) {
        for (int px = 0; px < R * 2; px++) {
            // Map pixel coordinates relative to the center of the sphere
            float x = (float)(px - R);
            float y = (float)(py - R);
            
            float z2 = R2 - x*x - y*y;
            
            if (z2 >= 0) { // If we are inside the circle
                float z = sqrt(z2);
                
                // Inverse Tilt (around Z axis)
                float x1 = x * cos_tilt - y * sin_tilt;
                float y1 = x * sin_tilt + y * cos_tilt;
                float z1 = z;
                
                // Inverse Spin (around Y axis)
                float x2 = x1 * cos_spin - z1 * sin_spin;
                float y2 = y1;
                float z2_rot = x1 * sin_spin + z1 * cos_spin;
                
                // Convert 3D point to Spherical Coordinates (Longitude & Latitude)
                float u = atan2(x2, z2_rot); 
                float v = asin(y2 / R);      
                
                // Map to a 16x16 Checkerboard grid (smaller squares)
                int u_check = (int)(floor((u + PI) / (PI / 8.0f))); 
                int v_check = (int)(floor((v + PI/2.0f) / (PI / 8.0f)));
                
                bool is_red = ((u_check + v_check) % 2) == 0;
                
                // Calculate simple directional lighting (shading)
                float light = 0.5f + 0.5f * (z / R); 
                
                Uint8 r = is_red ? 216 : 255;
                Uint8 g = is_red ? 0   : 255;
                Uint8 b = is_red ? 0   : 255;
                
                // Apply lighting
                r = (Uint8)(r * light);
                g = (Uint8)(g * light);
                b = (Uint8)(b * light);
                
                // Write pixel (ARGB8888 format)
                dst[py * (pitch / 4) + px] = (255 << 24) | (r << 16) | (g << 8) | b;
            } else {
                // Outside the circle, make pixel transparent
                dst[py * (pitch / 4) + px] = 0x00000000;
            }
        }
    }
    
    // Unlock and send back to the GPU
    SDL_UnlockTexture(ball->streaming_texture);
}

static void ball_render(Effect* self, SDL_Renderer* renderer) {
    AmigaBoingBall* ball = (AmigaBoingBall*)self;
    SDL_Rect dstrect = { (int)ball->x - ball->radius, (int)ball->y - ball->radius, ball->radius * 2, ball->radius * 2 };

    // --- The Amiga Drop Shadow Trick ---
    // We reuse the exact same texture, but tell the GPU to tint it black and make it semi-transparent
    SDL_Rect shadow_rect = dstrect;
    shadow_rect.x += 25; 
    shadow_rect.y += 25;
    
    SDL_SetTextureColorMod(ball->streaming_texture, 0, 0, 0);   // Tint Black
    SDL_SetTextureAlphaMod(ball->streaming_texture, 80);        // Alpha 80/255
    SDL_RenderCopy(renderer, ball->streaming_texture, NULL, &shadow_rect);

    // --- Render the Actual Ball ---
    // Reset hardware color tinting back to normal
    SDL_SetTextureColorMod(ball->streaming_texture, 255, 255, 255); 
    SDL_SetTextureAlphaMod(ball->streaming_texture, 255);
    SDL_RenderCopy(renderer, ball->streaming_texture, NULL, &dstrect);
}

static void ball_destroy(Effect* self) {
    AmigaBoingBall* ball = (AmigaBoingBall*)self;
    if (ball->streaming_texture) SDL_DestroyTexture(ball->streaming_texture);
    free(self);
}

static const EffectVTable ball_vtable = {
    .update = ball_update,
    .render = ball_render,
    .destroy = ball_destroy
};

// Constructor needs the renderer to create the streaming texture
Effect* create_amiga_ball(SDL_Renderer* renderer, float start_x, float start_y, Mix_Chunk* sound) {
    AmigaBoingBall* ball = malloc(sizeof(AmigaBoingBall));
    ball->base.vptr = &ball_vtable;
    ball->base.is_active = true;
    
    ball->x = start_x;
    ball->y = start_y;
    ball->vx = 350.0f; 
    ball->vy = 280.0f;
    ball->radius = 120; // Much larger to appreciate the 3D pixels
    ball->hit_sound = sound;
    
    ball->spin_angle = 0.0f;
    ball->tilt_angle = 0.26f; // ~15 degrees slant

    // Create a texture optimized for frequent CPU updates
    ball->streaming_texture = SDL_CreateTexture(renderer, 
                                                SDL_PIXELFORMAT_ARGB8888, 
                                                SDL_TEXTUREACCESS_STREAMING, 
                                                ball->radius * 2, 
                                                ball->radius * 2);
    
    // Enable transparency for the pixels outside the sphere radius
    SDL_SetTextureBlendMode(ball->streaming_texture, SDL_BLENDMODE_BLEND);

    return (Effect*)ball;
}


/* =========================================
 * 3. THE MAIN SDL ENGINE
 * ========================================= */

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) return 1;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("True 3D Amiga Boing Ball (C/OOP)", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    Mix_Music* bgm = Mix_LoadMUS("bgm.mod");
    Mix_Chunk* boing_sound = Mix_LoadWAV("boing.wav");
    if (bgm) Mix_PlayMusic(bgm, -1); 

    // Initialize Effect (Notice we pass the renderer into the constructor now)
    Effect* playlist[1];
    playlist[0] = create_amiga_ball(renderer, WINDOW_WIDTH/2, WINDOW_HEIGHT/2, boing_sound);

    bool running = true;
    SDL_Event event;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        Uint32 current_time = SDL_GetTicks();
        float delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        if (playlist[0]->is_active) playlist[0]->vptr->update(playlist[0], delta_time);

        // Classic Amiga Workbench Gray background
        SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
        SDL_RenderClear(renderer);

        if (playlist[0]->is_active) playlist[0]->vptr->render(playlist[0], renderer);

        SDL_RenderPresent(renderer);
    }

    playlist[0]->vptr->destroy(playlist[0]);

    if (boing_sound) Mix_FreeChunk(boing_sound);
    if (bgm) Mix_FreeMusic(bgm);
    
    Mix_CloseAudio();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}