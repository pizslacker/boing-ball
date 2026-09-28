#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For getopt

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Default Configuration
int screen_width = 800;
int screen_height = 600;
int ball_r = 100;
int target_fps = 60;
float gravity = 0.3f;
float bounce_dampening = 1.0f; // 1.0 = endless bounce

// Grid constraints
#define LON_SEGMENTS 16
#define LAT_SEGMENTS 8

float clamp(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// Draw the classic Amiga perspective grid with overlapping back wall
void draw_background_grid(SDL_Renderer* renderer) {
    // 1. Establish the floor's vanishing point behind the back wall
    float vp_x = screen_width / 2.0f;
    float vp_y = 150.0f; 
    
    int num_lines = 16;
    float spacing_at_bottom = 150.0f;
    
    // 2. Calculate exactly where the outermost floor line hits the screen edge (X=0)
    float outermost_x = vp_x - (num_lines / 2) * spacing_at_bottom;
    float dx = outermost_x - vp_x; 
    float dy = screen_height - vp_y;      
    
    // Find Y where X = 0 (t is the fraction along the line)
    float t = -vp_x / dx;
    int horizon = (int)(vp_y + t * dy);

    // Clear screen with base grey
    SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
    SDL_RenderClear(renderer);

    // --- Draw the Floor First ---
    SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);
    
    // Radiating perspective lines
    for (int i = -num_lines / 2; i <= num_lines / 2; i++) {
        float end_x = vp_x + i * spacing_at_bottom;
        SDL_RenderDrawLine(renderer, (int)vp_x, (int)vp_y, (int)end_x, screen_height);
    }

    // Horizontal perspective lines 
    float cur_y = vp_y + 1.0f;
    float step = 1.0f;
    while (cur_y <= screen_height) {
        if (cur_y >= horizon) {
            SDL_RenderDrawLine(renderer, 0, (int)cur_y, screen_width, (int)cur_y);
        }
        cur_y += step;
        step *= 1.15f; 
    }

    // --- Lift the Background Layer OVER the Floor ---
    SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
    SDL_Rect back_wall = { 0, 0, screen_width, horizon };
    SDL_RenderFillRect(renderer, &back_wall);

    // --- Draw the Background Pattern (Grid) ---
    SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);
    for (int x = 0; x <= screen_width; x += 50) {
        SDL_RenderDrawLine(renderer, x, 0, x, horizon);
    }
    for (int y = 0; y <= horizon; y += 50) {
        SDL_RenderDrawLine(renderer, 0, y, screen_width, y);
    }
    
    // Draw a definitive separator line between the wall and the floor
    SDL_RenderDrawLine(renderer, 0, horizon, screen_width, horizon);
}

// Procedurally generate the texture of the tilted, rotated sphere
void update_ball_texture(SDL_Texture* tex, float phase) {
    void* pixels;
    int pitch;
    SDL_LockTexture(tex, NULL, &pixels, &pitch);
    Uint32* dst = (Uint32*)pixels;

    float tilt = 15.0f * M_PI / 180.0f;
    float cos_tilt = cos(-tilt);
    float sin_tilt = sin(-tilt);
    
    float cos_phase = cos(-phase);
    float sin_phase = sin(-phase);

    for (int y = 0; y < ball_r * 2; y++) {
        for (int x = 0; x < ball_r * 2; x++) {
            float px = x - ball_r;
            float py = y - ball_r;
            float r2 = px * px + py * py;
            
            if (r2 <= ball_r * ball_r) {
                float pz = sqrt(ball_r * ball_r - r2);

                float x1 = px * cos_tilt - py * sin_tilt;
                float y1 = px * sin_tilt + py * cos_tilt;
                float z1 = pz;

                float x0 = x1 * cos_phase + z1 * sin_phase;
                float y0 = y1;
                float z0 = -x1 * sin_phase + z1 * cos_phase;

                float y_norm = clamp(y0 / ball_r, -1.0f, 1.0f);
                float lat = asin(y_norm);
                float lon = atan2(x0, z0);

                int lon_idx = (int)floor((lon + M_PI) / (2 * M_PI) * LON_SEGMENTS);
                int lat_idx = (int)floor((lat + M_PI / 2) / M_PI * LAT_SEGMENTS);

                bool is_red = (lon_idx + lat_idx) % 2 == 0;
                dst[y * (pitch / 4) + x] = is_red ? 0xFFFF0000 : 0xFFFFFFFF; 
            } else {
                dst[y * (pitch / 4) + x] = 0x00000000; 
            }
        }
    }
    SDL_UnlockTexture(tex);
}

void print_help(const char* prog_name) {
    printf("Amiga Boing Ball 1985 Clone\n\n");
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -w <width>      Window width (default: 800)\n");
    printf("  -H <height>     Window height (default: 600)\n");
    printf("  -r <radius>     Ball radius (default: 100)\n");
    printf("  -g <gravity>    Gravity strength (default: 0.3)\n");
    printf("  -d <dampening>  Bounce dampening multiplier, 1.0 is endless (default: 1.0)\n");
    printf("  -f <fps>        Target framerate (default: 60)\n");
    printf("  -h              Show this help message\n");
}

int main(int argc, char* argv[]) {
    int opt;
    // Parse command line arguments
    while ((opt = getopt(argc, argv, "w:H:r:g:d:f:h")) != -1) {
        switch (opt) {
            case 'w': screen_width = atoi(optarg); break;
            case 'H': screen_height = atoi(optarg); break;
            case 'r': ball_r = atoi(optarg); break;
            case 'g': gravity = atof(optarg); break;
            case 'd': bounce_dampening = atof(optarg); break;
            case 'f': target_fps = atoi(optarg); break;
            case 'h': print_help(argv[0]); return 0;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Amiga Boing Ball 1985", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          screen_width, screen_height, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* ball_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                              SDL_TEXTUREACCESS_STREAMING, ball_r * 2, ball_r * 2);
    SDL_SetTextureBlendMode(ball_tex, SDL_BLENDMODE_BLEND);

    SDL_Texture* shadow_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                                SDL_TEXTUREACCESS_STATIC, ball_r * 2, ball_r * 2);
    SDL_SetTextureBlendMode(shadow_tex, SDL_BLENDMODE_BLEND);
    
    // Generate static shadow
    Uint32* shadow_pixels = (Uint32*)malloc(4 * ball_r * 2 * ball_r * 2);
    for (int y = 0; y < ball_r * 2; y++) {
        for (int x = 0; x < ball_r * 2; x++) {
            float px = x - ball_r;
            float py = y - ball_r;
            shadow_pixels[y * (ball_r * 2) + x] = (px * px + py * py <= ball_r * ball_r) ? 0x55000000 : 0x00000000;
        }
    }
    SDL_UpdateTexture(shadow_tex, NULL, shadow_pixels, ball_r * 2 * 4);
    free(shadow_pixels);

    // Physics State
    float ball_x = screen_width / 2.0f;
    float ball_y = screen_height / 4.0f;
    float vel_x = 4.0f;
    float vel_y = 0.0f;
    float phase = 0.0f;
    float rot_vel = 0.06f;

    bool quit = false;
    SDL_Event e;
    Uint32 frame_delay = 1000 / target_fps;

    while (!quit) {
        Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                quit = true;
            }
        }

        // --- Physics Update ---
        vel_y += gravity;
        ball_x += vel_x;
        ball_y += vel_y;
        phase += rot_vel;

        if (ball_x + ball_r > screen_width) {
            ball_x = screen_width - ball_r;
            vel_x = -vel_x;
            rot_vel = -rot_vel; 
        } else if (ball_x - ball_r < 0) {
            ball_x = ball_r;
            vel_x = -vel_x;
            rot_vel = -rot_vel;
        }

        if (ball_y + ball_r > screen_height) {
            ball_y = screen_height - ball_r;
            vel_y = -vel_y * bounce_dampening;
        }

        // --- Rendering ---
        draw_background_grid(renderer);
        update_ball_texture(ball_tex, phase);

        // Draw Shadow
        SDL_Rect shadow_rect = { (int)ball_x - ball_r + 30, (int)ball_y - ball_r + 20, ball_r * 2, ball_r * 2 };
        SDL_RenderCopy(renderer, shadow_tex, NULL, &shadow_rect);

        // Draw Ball
        SDL_Rect ball_rect = { (int)ball_x - ball_r, (int)ball_y - ball_r, ball_r * 2, ball_r * 2 };
        SDL_RenderCopy(renderer, ball_tex, NULL, &ball_rect);

        SDL_RenderPresent(renderer);

        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_delay > frame_time) {
            SDL_Delay(frame_delay - frame_time);
        }
    }

    SDL_DestroyTexture(ball_tex);
    SDL_DestroyTexture(shadow_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}