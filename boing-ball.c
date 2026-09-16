#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
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
    SDL_Texture* streaming_texture; 
    float spin_angle;
    float tilt_angle;
} AmigaBoingBall;

static void ball_update(Effect* self, float dt) {
    AmigaBoingBall* ball = (AmigaBoingBall*)self;

    ball->x += ball->vx * dt;
    ball->y += ball->vy * dt;
    ball->spin_angle += 3.0f * dt;

    bool hit_wall = false;
    if (ball->x <= ball->radius) { ball->x = ball->radius; ball->vx *= -1; hit_wall = true; }
    else if (ball->x >= WINDOW_WIDTH - ball->radius) { ball->x = WINDOW_WIDTH - ball->radius; ball->vx *= -1; hit_wall = true; }
    
    if (ball->y <= ball->radius) { ball->y = ball->radius; ball->vy *= -1; hit_wall = true; }
    else if (ball->y >= WINDOW_HEIGHT - ball->radius) { ball->y = WINDOW_HEIGHT - ball->radius; ball->vy *= -1; hit_wall = true; }

    if (hit_wall && ball->hit_sound != NULL) Mix_PlayChannel(-1, ball->hit_sound, 0);

    void* pixels;
    int pitch;
    SDL_LockTexture(ball->streaming_texture, NULL, &pixels, &pitch);
    Uint32* dst = (Uint32*)pixels;

    int R = ball->radius;
    int R2 = R * R;
    float cos_tilt = cos(-ball->tilt_angle);
    float sin_tilt = sin(-ball->tilt_angle);
    float cos_spin = cos(-ball->spin_angle);
    float sin_spin = sin(-ball->spin_angle);

    for (int py = 0; py < R * 2; py++) {
        for (int px = 0; px < R * 2; px++) {
            float x = (float)(px - R);
            float y = (float)(py - R);
            float z2 = R2 - x*x - y*y;
            
            if (z2 >= 0) {
                float z = sqrt(z2);
                float x1 = x * cos_tilt - y * sin_tilt;
                float y1 = x * sin_tilt + y * cos_tilt;
                float z1 = z;
                float x2 = x1 * cos_spin - z1 * sin_spin;
                float y2 = y1;
                float z2_rot = x1 * sin_spin + z1 * cos_spin;
                float u = atan2(x2, z2_rot); 
                float v = asin(y2 / R);      
                
                int u_check = (int)(floor((u + PI) / (PI / 8.0f))); 
                int v_check = (int)(floor((v + PI/2.0f) / (PI / 8.0f))); 
                bool is_red = ((u_check + v_check) % 2) == 0;
                
                float light = 0.5f + 0.5f * (z / R); 
                Uint8 r = is_red ? 216 : 255;
                Uint8 g = is_red ? 0   : 255;
                Uint8 b = is_red ? 0   : 255;
                r = (Uint8)(r * light);
                g = (Uint8)(g * light);
                b = (Uint8)(b * light);
                
                dst[py * (pitch / 4) + px] = (255 << 24) | (r << 16) | (g << 8) | b;
            } else {
                dst[py * (pitch / 4) + px] = 0x00000000;
            }
        }
    }
    SDL_UnlockTexture(ball->streaming_texture);
}

static void ball_render(Effect* self, SDL_Renderer* renderer) {
    AmigaBoingBall* ball = (AmigaBoingBall*)self;
    SDL_Rect dstrect = { (int)ball->x - ball->radius, (int)ball->y - ball->radius, ball->radius * 2, ball->radius * 2 };

    SDL_Rect shadow_rect = dstrect;
    shadow_rect.x += 25; shadow_rect.y += 25;
    SDL_SetTextureColorMod(ball->streaming_texture, 0, 0, 0);   
    SDL_SetTextureAlphaMod(ball->streaming_texture, 80);        
    SDL_RenderCopy(renderer, ball->streaming_texture, NULL, &shadow_rect);

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

Effect* create_amiga_ball(SDL_Renderer* renderer, float start_x, float start_y, Mix_Chunk* sound) {
    AmigaBoingBall* ball = malloc(sizeof(AmigaBoingBall));
    ball->base.vptr = &ball_vtable;
    ball->base.is_active = true;
    ball->x = start_x;
    ball->y = start_y;
    ball->vx = 350.0f; 
    ball->vy = 280.0f;
    ball->radius = 100;
    ball->hit_sound = sound;
    ball->spin_angle = 0.0f;
    ball->tilt_angle = 0.26f;
    ball->streaming_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ball->radius * 2, ball->radius * 2);
    SDL_SetTextureBlendMode(ball->streaming_texture, SDL_BLENDMODE_BLEND);
    return (Effect*)ball;
}

/* =========================================
 * 3. DERIVED CLASS B (XEyes)
 * ========================================= */

// Software rasterization helper for primitive ellipses
static void draw_filled_ellipse(SDL_Renderer* renderer, int cx, int cy, int rx, int ry) {
    for (int dy = -ry; dy <= ry; dy++) {
        // Calculate the width of the ellipse at this specific Y row
        int dx = (int)(rx * sqrt(1.0 - (double)(dy*dy)/(ry*ry)));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

typedef struct {
    Effect base;
    int x, y; // The center point between the two eyes
    AmigaBoingBall* target; // Pointer to the ball to track its coordinates
    float l_px, l_py; // Left pupil position
    float r_px, r_py; // Right pupil position
} XEyesEffect;

static void xeyes_update(Effect* self, float dt) {
    (void)dt; // Suppress unused parameter warning
    XEyesEffect* eyes = (XEyesEffect*)self;
    if (!eyes->target) return;

    // Dimensions
    int rx = 25, ry = 40; // The outer eye radii
    int pr = 10;          // The pupil radius
    int mx = rx - pr - 2; // Max pupil offset X (bound to ellipse)
    int my = ry - pr - 2; // Max pupil offset Y (bound to ellipse)
    
    int l_cx = eyes->x - 28; // Left eye true center
    int r_cx = eyes->x + 28; // Right eye true center
    int cy = eyes->y;

    // Grab target coordinates dynamically
    float tx = eyes->target->x;
    float ty = eyes->target->y;

    // --- Left Eye Pupil Tracking ---
    float l_dx = tx - l_cx;
    float l_dy = ty - cy;
    // Map vector into the bounds of our specific ellipse
    float l_dist = sqrt((l_dx/mx)*(l_dx/mx) + (l_dy/my)*(l_dy/my));
    if (l_dist > 1.0f) { l_dx /= l_dist; l_dy /= l_dist; } // Clamp to edge
    
    eyes->l_px = l_cx + l_dx;
    eyes->l_py = cy + l_dy;

    // --- Right Eye Pupil Tracking ---
    float r_dx = tx - r_cx;
    float r_dy = ty - cy;
    float r_dist = sqrt((r_dx/mx)*(r_dx/mx) + (r_dy/my)*(r_dy/my));
    if (r_dist > 1.0f) { r_dx /= r_dist; r_dy /= r_dist; } // Clamp to edge
    
    eyes->r_px = r_cx + r_dx;
    eyes->r_py = cy + r_dy;
}

static void xeyes_render(Effect* self, SDL_Renderer* renderer) {
    XEyesEffect* eyes = (XEyesEffect*)self;

    // 1. Render outlines (Slightly larger black ellipse)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    draw_filled_ellipse(renderer, eyes->x - 28, eyes->y, 27, 42);
    draw_filled_ellipse(renderer, eyes->x + 28, eyes->y, 27, 42);

    // 2. Render Sclera (White inner eye)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    draw_filled_ellipse(renderer, eyes->x - 28, eyes->y, 25, 40);
    draw_filled_ellipse(renderer, eyes->x + 28, eyes->y, 25, 40);

    // 3. Render Pupils (Black, at dynamically calculated positions)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    draw_filled_ellipse(renderer, (int)eyes->l_px, (int)eyes->l_py, 10, 10);
    draw_filled_ellipse(renderer, (int)eyes->r_px, (int)eyes->r_py, 10, 10);
}

static void xeyes_destroy(Effect* self) {
    free(self);
}

static const EffectVTable xeyes_vtable = {
    .update = xeyes_update,
    .render = xeyes_render,
    .destroy = xeyes_destroy
};

Effect* create_xeyes(int center_x, int center_y, AmigaBoingBall* target) {
    XEyesEffect* eyes = malloc(sizeof(XEyesEffect));
    eyes->base.vptr = &xeyes_vtable;
    eyes->base.is_active = true;
    
    eyes->x = center_x;
    eyes->y = center_y;
    eyes->target = target;
    
    return (Effect*)eyes;
}

/* =========================================
 * 4. DERIVED CLASS C (WorkbenchCurtain)
 * ========================================= */

typedef struct {
    Effect base;
    SDL_Texture* image;
    float y_offset;
    float velocity; 
    float delay_timer;
} WorkbenchCurtain;

static void curtain_update(Effect* self, float dt) {
    WorkbenchCurtain* curtain = (WorkbenchCurtain*)self;
    
    if (curtain->delay_timer > 0.0f) {
        curtain->delay_timer -= dt;
        return;
    }

    curtain->velocity += 1500.0f * dt; 
    curtain->y_offset += curtain->velocity * dt;

    if (curtain->y_offset > WINDOW_HEIGHT) {
        curtain->base.is_active = false;
    }
}

static void curtain_render(Effect* self, SDL_Renderer* renderer) {
    WorkbenchCurtain* curtain = (WorkbenchCurtain*)self;
    if (!curtain->image) return;

    SDL_Rect dstrect = { 0, (int)curtain->y_offset, WINDOW_WIDTH, WINDOW_HEIGHT };
    SDL_RenderCopy(renderer, curtain->image, NULL, &dstrect);
}

static void curtain_destroy(Effect* self) {
    free(self);
}

static const EffectVTable curtain_vtable = {
    .update = curtain_update,
    .render = curtain_render,
    .destroy = curtain_destroy
};

Effect* create_workbench_curtain(SDL_Texture* wb_texture) {
    WorkbenchCurtain* curtain = malloc(sizeof(WorkbenchCurtain));
    curtain->base.vptr = &curtain_vtable;
    curtain->base.is_active = true;
    curtain->image = wb_texture;
    curtain->y_offset = 0.0f;
    curtain->velocity = 0.0f;
    curtain->delay_timer = 1.5f;
    return (Effect*)curtain;
}

/* =========================================
 * 5. THE MAIN SDL ENGINE
 * ========================================= */

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) return 1;
    
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(img_flags) & img_flags)) return 1;

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("True 3D Amiga Boing Ball in C / SDL2 / OOP", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    Mix_Music* bgm = Mix_LoadMUS("bgm.mod");
    Mix_Chunk* boing_sound = Mix_LoadWAV("boing.wav");
    if (bgm) Mix_PlayMusic(bgm, -1); 

    SDL_Texture* workbench_tex = IMG_LoadTexture(renderer, "workbench.png");

    // We now have 3 items in our engine playlist
    Effect* playlist[3];
    
    // 0. The Ball
    playlist[0] = create_amiga_ball(renderer, WINDOW_WIDTH/2, WINDOW_HEIGHT/2, boing_sound);
    
    // 1. The Xeyes
    // Center is set to (20, 600). The left eye renders at -8, making it peek in from off-screen left.
    // The bottom of the eyes render at 642, making them peek up from off-screen bottom.
    // Shifts the eyes rightward and upward so the full sclera and pupils stay visible
    playlist[1] = create_xeyes(45, WINDOW_HEIGHT - 20, (AmigaBoingBall*)playlist[0]);
    
    // 2. The Curtain (Renders on top of everything until it falls away)
    playlist[2] = create_workbench_curtain(workbench_tex);

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

        // Polymorphic Updates
        for (int i = 0; i < 3; i++) {
            if (playlist[i]->is_active) playlist[i]->vptr->update(playlist[i], delta_time);
        }

        SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
        SDL_RenderClear(renderer);

        // Polymorphic Rendering
        for (int i = 0; i < 3; i++) {
            if (playlist[i]->is_active) playlist[i]->vptr->render(playlist[i], renderer);
        }

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    for (int i = 0; i < 3; i++) {
        playlist[i]->vptr->destroy(playlist[i]);
    }

    if (workbench_tex) SDL_DestroyTexture(workbench_tex);
    if (boing_sound) Mix_FreeChunk(boing_sound);
    if (bgm) Mix_FreeMusic(bgm);
    
    Mix_CloseAudio();
    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}