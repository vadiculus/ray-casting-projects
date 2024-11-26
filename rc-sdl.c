#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_image.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <complex.h>
#include <time.h>

#define PI 3.141592654
#define BLOCK_SIZE 50
#define BOOST_SPEED 0.8
#define STOP_SPEED 1
#define ROTATION_SPEED 3.5
#define PLAYER_FOV 70
#define BOARD_HEIGHT 20
#define BOARD_WIDTH 20
#define MAX_RAY_ITER 100
#define RAY_COUNT 1270
#define FPS 120
#define PLAYER_SPEED 2
#define PLAYER_CAM_Z 50
#define DELTA (PLAYER_FOV / (float)RAY_COUNT)
#define PIX_SIZE (int)(WIN_WIDTH / (float)RAY_COUNT)

#define CLEAR(x) memset(x, 0, sizeof(x))

typedef struct {
    float length;
} Ray;

typedef struct {
    float x;
    float y;
    float FOV;
    float angle;
    int rays_count;
    float speed;
    float col_w;
    float col_h;
    Ray rays[RAY_COUNT];
} Player;

typedef struct {
    SDL_Texture *texture;
    int w;
    int h;
} Texture;

typedef struct {
    int x;
    int y;
    int z;
    Texture *texture;
    float distance;
    float col_w;
    float col_h;
} GObject;  // <----- Game Object

typedef enum {
    CONCRETE,
    BRICK,
    EMPTY
} BlockType;

typedef struct {
    BlockType block_type;
} Block;

typedef struct {
    float x;
    float y;
    float length;
} new_slice_cords;

int mouse_x, mouse_y, old_mx, old_my;

// Creating Window and Renderer
int WIN_WIDTH = 1270;
int WIN_HEIGHT = 720;

SDL_Window *window;
SDL_Renderer *renderer;

bool exit_mode = false;

Player player = {400, 200, PLAYER_FOV, 90,  RAY_COUNT, 0, 6, 6};
bool run_state = false;
float cent_angle;
GObject gobjects[50];
GObject *sorted_gobjects[50];
int gobjects_cnt;

float cam_shake = 0;
bool shake_up = true;

char board_draw[BOARD_HEIGHT][BOARD_WIDTH + 1] = \
    {
        "####################",
        "#                  #",
        "#######  #  #####  #",
        "#                  #",
        "####               #",
        "#  # #  00 00      #",
        "#  #   0     0     #",
        "#      0#### 0     #",
        "#      0     0  #  #",
        "#      0 ####0  #  #",
        "####   0      0#   #",
        "#      0 000       #",
        "####      ### ######",
        "#  #               #",
        "#  # 00            #",
        "#             #    #",
        "#             0    #",
        "########0###########",
    };

SDL_Surface *wall_sf, *concrete_sf, *shotgun_sf, *floor_sf, *mob_sf, *barrel_sf;
Texture wall_tx, concrete_tx, floor_tx, shotgun_tx, mob_tx, barrel_tx;

Block board[BOARD_HEIGHT][BOARD_WIDTH];

int key_down_arr[20];
int key_up_arr[20];

double degToRad(float x){
    return (double)((x * PI) / 180);
}

double radToDeg(float x){
    return (double)((x * 180) / PI);
}

void getrayxy(float length, float angle, float *x, float *y, bool is_x_intercept, bool wall_alignment){
    float real_angle;


    if (angle > 360) angle -= 360;
    if (angle < 0) angle += 360;
    
    if (angle > 270){
        real_angle = angle - 270;
    } else if (angle > 180) {
        real_angle = angle - 180;
    } else if (angle > 90) {
        real_angle = angle - 90;
    } else {
        real_angle = angle;
    }

    double radians = degToRad(real_angle);
    if (angle > 270){
        *x = length * sin(radians);
        *y = length * cos(radians) * -1;
    } else if (angle > 180) {
        *x = length * cos(radians) * -1;
        *y = length * sin(radians) * -1;
    } else if (angle > 90) {
        *x = length * sin(radians) * -1;
        *y = length * cos(radians);
    } else if (angle > 0) {
        *x = length * cos(radians);
        *y = length * sin(radians);
    }
}

void initSDL(){
    if (SDL_Init(SDL_INIT_VIDEO) < 0){
        printf("SDL ERROR: %s", SDL_GetError());
    }

    window = SDL_CreateWindow("DOOM", SDL_WINDOWPOS_UNDEFINED, 0, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

int fCtI(float x){ // <---- From Cordinate To Index
    return (int)(x / BLOCK_SIZE);
}

void insert_to_key_arr(int *key_arr, SDL_Event event){
    int i;

    for (i=0; i < 20; i++){
        if (key_arr[i] == event.key.keysym.sym || key_arr[i] == 0) {
            key_arr[i] = 0;
            break;
        }
    }
    key_arr[i] = event.key.keysym.sym;
    for (i=i+1; i < 20; i++){
        if (key_arr[i] == event.key.keysym.sym){
            key_arr[i] = 0;
            break;
        }
    }
}

void doInput(){
    SDL_Event event;
    int i, j;

    while (SDL_PollEvent(&event)) {
        
        if (event.type == SDL_QUIT){
            exit(EXIT_SUCCESS);
        } else if (event.type == SDL_MOUSEMOTION){
            if (!exit_mode){
                SDL_GetMouseState(&mouse_x, &mouse_y);

                player.angle -= (WIN_WIDTH / 2 - mouse_x) * 0.1;
                cent_angle = player.angle + PLAYER_FOV / 2;
                SDL_WarpMouseInWindow(window, WIN_WIDTH/2, WIN_HEIGHT/2);
            }

        } else if (event.type == SDL_KEYDOWN){
            insert_to_key_arr(key_down_arr, event);
        } else if (event.type == SDL_KEYUP){
            for (i=0; i < 20; i++){
                if (key_down_arr[i] == event.key.keysym.sym) {
                    key_down_arr[i] = 0;
                    break;
                }
            }

            insert_to_key_arr(key_up_arr, event);
        }
    }
}

float movement_angle;

bool check_player_collision(float x, float y){
    if (board[fCtI(y - player.col_h / 2)][fCtI(x - player.col_w / 2)].block_type != EMPTY) return true;
    if (board[fCtI(y + player.col_h / 2)][fCtI(x - player.col_w / 2)].block_type != EMPTY) return true;
    if (board[fCtI(y - player.col_h / 2)][fCtI(x + player.col_w / 2)].block_type != EMPTY) return true;
    if (board[fCtI(y + player.col_h / 2)][fCtI(x + player.col_w / 2)].block_type != EMPTY) return true;
    for (int i = 0; i < gobjects_cnt; i++){
        if (gobjects[i].x - gobjects[i].col_w / 2 < x && x < gobjects[i].x + gobjects[i].col_w / 2 && 
        gobjects[i].y - gobjects[i].col_h / 2 < y && y < gobjects[i].y + gobjects[i].col_h / 2) return true;
    } 
    return false;

}

void movement(int button){
    float new_x, new_y;
    new_x = new_y = 0;
    bool move_state = false;

    if (button == SDLK_w){
            movement_angle = cent_angle;
            move_state = true;
    } else if (button == SDLK_s){
        movement_angle = cent_angle - 180;
        move_state = true;
    } else if (button == SDLK_d){
        movement_angle = cent_angle + 90;
        move_state = true;
    } else if (button == SDLK_a){
        movement_angle = cent_angle - 90;
        move_state = true;
    }

    if (move_state == false) player.speed = 0;
    else player.speed = PLAYER_SPEED;
    
    if (run_state == true) player.speed *= 2;



    getrayxy(player.speed, movement_angle, &new_x, &new_y, NULL, false);
    if (!check_player_collision(player.x + new_x, player.y + new_y)){
        player.x += new_x;
        player.y += new_y;
        if (move_state == true){
            if (shake_up){
                if (cam_shake >= 20) shake_up = false;
                else cam_shake += PLAYER_SPEED / 2.0;
                
            } else {
                if (cam_shake <= -20) shake_up = true;
                else cam_shake -= PLAYER_SPEED / 2.0;
                
            }
        }
    }

}


void input_handler(){
    bool exit_pressed = false;

    if (player.angle >= 360){
        player.angle = 1;
    } else if (player.angle < 0){
        player.angle = 359;
    }

    for (int i = 0; i < 20; i++){
        if (key_down_arr[i] == 0) break;
        
        else if (key_down_arr[i] == SDLK_ESCAPE){
            if (!exit_pressed){
                exit_mode = !exit_mode;
                SDL_ShowCursor(exit_mode);
            }
            exit_pressed = true;
            
        } else if (key_down_arr[i] == SDLK_w || key_down_arr[i] == SDLK_s || key_down_arr[i] == SDLK_a || key_down_arr[i] == SDLK_d ){
            movement(key_down_arr[i]);
        } else if (key_down_arr[i] == SDLK_LSHIFT) {
            run_state = true;
        }
    }
    for (int i = 0; i < 20; i++){
        if (key_up_arr[i] == 0) break;
        else if (key_up_arr[i] == SDLK_LSHIFT){
            run_state = false;
        }
        key_up_arr[i] = 0;
    }
}

void renderTextureClipped(SDL_Renderer* cur_renderer, SDL_Texture* texture, SDL_Rect* clip, SDL_Rect* rect) {
    SDL_RenderCopy(cur_renderer, texture, clip, rect);
}

float get_next_ray_length(float x, float y, float angle, bool * is_x_intercept){
    float real_angle;
    float new_x, new_y;
    float length_x, length_y;
    // int ray_iter;
    if (angle > 360) angle -= 360;
    if (angle < 0) angle += 360;

    if (angle > 270){
        real_angle = angle - 270;
    } else if (angle > 180) {
        real_angle = angle - 180;
    } else if (angle > 90) {
        real_angle = angle - 90;
    } else {
        real_angle = angle;
    }

    double radians = degToRad(real_angle);
    if (angle > 270){
        new_x = ((int)(x / BLOCK_SIZE) * BLOCK_SIZE + BLOCK_SIZE + 0.001) - player.x;
        new_y = player.y - ((int)(y / BLOCK_SIZE) * BLOCK_SIZE - 0.001);
        length_x = new_x / sin(radians);
        length_y = new_y / cos(radians);
    } else if (angle > 180) {
        new_x = player.x - ((int)(x / BLOCK_SIZE) * BLOCK_SIZE - 0.001);
        new_y = player.y - ((int)(y / BLOCK_SIZE) * BLOCK_SIZE - 0.001);
        length_x = new_x / cos(radians);
        length_y = new_y / sin(radians);
    } else if (angle > 90) {
        new_x = player.x - (fCtI(x) * BLOCK_SIZE - 0.001);
        new_y = (fCtI(y) * BLOCK_SIZE + BLOCK_SIZE + 0.001) - player.y;
        length_x = new_x / sin(radians);
        length_y = new_y / cos(radians);
    } else if (angle > 0) {
        new_x = ((int)(x / BLOCK_SIZE) * BLOCK_SIZE + BLOCK_SIZE + 0.001) - player.x;
        new_y = ((int)(y / BLOCK_SIZE) * BLOCK_SIZE + BLOCK_SIZE + 0.001) - player.y;
        length_x = new_x / cos(radians);
        length_y = new_y / sin(radians);
    }

    if (length_x >= length_y){
        *is_x_intercept = false;
        return length_y;
    } else {
        *is_x_intercept = true;
        return length_x;
    }
}

void draw_floor(){
    float bright;
    int i;

    for (i = 0; i < WIN_HEIGHT / 2 + cam_shake * -1; i++){
        bright = (float)i / (WIN_HEIGHT / 2) * 100;
        if (bright > 100) bright = 100;
        SDL_SetRenderDrawColor(renderer, bright, bright, bright, 255);
        SDL_RenderDrawLine(renderer, 0, WIN_HEIGHT / 2 + cam_shake + i, WIN_WIDTH,  WIN_HEIGHT / 2 + cam_shake + i);
    }
    for (i = WIN_HEIGHT / 2 + i; i < WIN_HEIGHT; i++){
        SDL_SetRenderDrawColor(renderer, bright, bright, bright, 255);
        SDL_RenderDrawLine(renderer, 0, cam_shake + i, WIN_WIDTH,  cam_shake + i);
    }

}

void sort_objects_by_distance(){
    CLEAR(sorted_gobjects);
    for (int i = 0; i < gobjects_cnt; i++) sorted_gobjects[i] = &gobjects[i];
    GObject *cur_gobj;

    for (int i = 0; i < gobjects_cnt; i++){
        for (int j = 0; j < gobjects_cnt; j++){
            if (sorted_gobjects[j]->distance < sorted_gobjects[i]->distance){
                cur_gobj = sorted_gobjects[j];
                sorted_gobjects[j] = sorted_gobjects[i];
                sorted_gobjects[i] = cur_gobj;
            }
        }
    }
}

void draw_objects(){
    float angleTO;
    int ray_index;
    SDL_Rect clip;
    GObject *gobj;
    SDL_Rect obj_rect;
    sort_objects_by_distance();
    for (int gi = 0; gi < gobjects_cnt; gi++){
        gobj = sorted_gobjects[gi];
        if (gobj == 0) continue;
        angleTO = radToDeg(atan((double)((gobj->y - player.y) / (gobj->x - player.x))));

        if (gobj->x > player.x && gobj->y < player.y) angleTO += 360;
        else if (gobj->x < player.x && gobj->y < player.y) angleTO += 180;
        else if (gobj->x < player.x && gobj->y > player.y) angleTO += 180;
        
        float player_angle = player.angle > 360 - player.FOV && angleTO < 360 - player.FOV ? player.angle - 360 : player.angle;
        gobj->distance = fabs(sqrt(pow(gobj->y - player.y, 2) + pow(gobj->x - player.x, 2)));
        int obj_screen_pos = (angleTO - player_angle) / DELTA * (WIN_WIDTH / RAY_COUNT);
        int obj_h = WIN_HEIGHT / (gobj->distance * 0.018); 
        int obj_w = obj_h * (gobj->texture->w / (float)gobj->texture->h);
        int obj_y = WIN_HEIGHT / 2 - obj_h /2;
        obj_y += radToDeg(atan(gobj->z / gobj->distance)) * 5;
        int bright = 255 - (gobj->distance / (800.0) * 255);
        if (bright < 0) bright = 0;

        for (int i = 0; i < obj_w; i += PIX_SIZE){
            ray_index = (int)((obj_screen_pos + i - obj_w / 2) / PIX_SIZE);
            if (ray_index < 0 || ray_index > RAY_COUNT - 1) continue;
            if (player.rays[ray_index].length > gobj->distance){

                clip.x = (int)((float)i / (float)obj_w * mob_tx.w);
                clip.y = 0;
                clip.w = 1;
                clip.h = gobj->texture->h;
                obj_rect.h = obj_h;
                obj_rect.w = PIX_SIZE;
                obj_rect.x = obj_screen_pos + i - obj_w / 2;
                obj_rect.y = obj_y + cam_shake;
                SDL_SetTextureColorMod(gobj->texture->texture, bright, bright , bright);
                SDL_RenderCopy(renderer, gobj->texture->texture, &clip, &obj_rect);
            }
        }
    }
}

void prepareScene(){
    float ray_angle = 0;
    float ray_x, ray_y;
    int bright;
    int wall_h;
    Block *cur_block;
    SDL_Rect rect;
    SDL_Rect tx_rect = {0, 0, 100, 100};
    rect.w = PIX_SIZE;
    bool is_x_intercept = false;
    int ray_iter = 0;
    Texture *cur_tx;

    SDL_RenderClear(renderer);

    float cent_angle = player.angle + player.FOV / 2;


    draw_floor();

    for(int i=0; i < player.rays_count; i++){  
        player.rays[i].length = 0;
        ray_iter = 0;

        ray_angle = player.angle + DELTA * (i+1);
        if (ray_angle > 360) ray_angle = ray_angle - 360;

        while (player.rays[i].length <= 2000 && ray_iter < MAX_RAY_ITER) {
            getrayxy(player.rays[i].length, ray_angle, &ray_x, &ray_y, is_x_intercept, true);
            ray_x = ray_x + player.x;
            ray_y = ray_y + player.y;

            if (is_x_intercept){
                ray_x = (int)(ray_x / BLOCK_SIZE) * BLOCK_SIZE;
            } else {
                ray_y = (int)(ray_y / BLOCK_SIZE) * BLOCK_SIZE;
            }

            if(ray_x < 0) break;
            if(ray_y < 0) break;

            if (ray_x >= BOARD_WIDTH * BLOCK_SIZE || ray_x < 0 || ray_y >= BOARD_HEIGHT * BLOCK_SIZE || ray_y < 0){
                player.rays[i].length = 0;
                break;
            }

            cur_block = &board[(int)(ray_y/BLOCK_SIZE)][(int)(ray_x/BLOCK_SIZE)];
            if (cur_block->block_type != EMPTY){
                if (cur_block->block_type == BRICK)
                    cur_tx = &wall_tx;
                else cur_tx = &concrete_tx;

                wall_h = WIN_HEIGHT / ((player.rays[i].length * cos(degToRad(ray_angle - cent_angle))) * 0.018);
                // 0.018
                if (wall_h < 0){
                    wall_h = 0;
                }

                bright = 255 - (player.rays[i].length / (800.0) * 255);
                if (bright < 0) bright = 0;

                rect.y = WIN_HEIGHT/2 - wall_h/2 + cam_shake;
                rect.x = i*rect.w;
                rect.h = wall_h;

                tx_rect.w = 1;
                tx_rect.h = cur_tx->h;
                if (!is_x_intercept)
                    tx_rect.x = (int)(fabs((ray_x - fCtI(ray_x) * BLOCK_SIZE) / BLOCK_SIZE * cur_tx->w));
                else
                    tx_rect.x = (int)(fabs((ray_y - fCtI(ray_y)  * BLOCK_SIZE) / BLOCK_SIZE * cur_tx->w));
                
                tx_rect.y = 0;

                SDL_SetTextureColorMod(cur_tx->texture, bright, bright , bright);

                renderTextureClipped(renderer, cur_tx->texture, &tx_rect, &rect);
                break;
            } 

            player.rays[i].length = get_next_ray_length(ray_x, ray_y, ray_angle, &is_x_intercept);
            ray_iter++;
        }
    }

    draw_objects();
    SDL_RenderCopy(renderer, shotgun_tx.texture, NULL, &(SDL_Rect){WIN_WIDTH / 2 - 200 / 2, WIN_HEIGHT - 200, 200, 200});

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

void presentScene(){
    SDL_RenderPresent(renderer);
}

void print_exit(){
    float ray_x, ray_y;
    float angle;
    for (int i = player.rays_count - 3; i < player.rays_count; i++){
        angle = player.angle + DELTA * (i+1);
        getrayxy(player.rays[i].length, angle, &ray_x, &ray_y, NULL, false);
        printf("---------");
        printf("l %f : a %f\n", player.rays[i].length, angle);
        printf("%f:%f\n", player.x + ray_x, player.y + ray_y);
        printf("%d:%d\n", (int)((player.x + ray_x) / BLOCK_SIZE), (int)((player.y + ray_y) / BLOCK_SIZE));
    }


    printf("player: %f:%f\n", player.x, player.y);
    printf("player_angle: %f:%f\n", player.angle, player.angle + PLAYER_FOV);
    printf("%f\n", DELTA);
    for (int i = 0; i < 3; i++) printf("%f\n", sorted_gobjects[i]->distance);
}

void create_textures(){
    wall_sf = IMG_Load("wall.png");
    concrete_sf = IMG_Load("stone.png");
    shotgun_sf = IMG_Load("shotgun.png");
    floor_sf = IMG_Load("floor.png");
    mob_sf = IMG_Load("mob.png");
    barrel_sf = IMG_Load("barrel.png");

    wall_tx.texture = SDL_CreateTextureFromSurface(renderer, wall_sf);
    concrete_tx.texture = SDL_CreateTextureFromSurface(renderer, concrete_sf);
    floor_tx.texture = SDL_CreateTextureFromSurface(renderer, concrete_sf);
    shotgun_tx.texture = SDL_CreateTextureFromSurface(renderer, shotgun_sf);
    mob_tx.texture = SDL_CreateTextureFromSurface(renderer, mob_sf);
    barrel_tx.texture = SDL_CreateTextureFromSurface(renderer, barrel_sf);
    
    SDL_QueryTexture(wall_tx.texture, NULL, NULL, &wall_tx.w, &wall_tx.h);
    SDL_QueryTexture(concrete_tx.texture, NULL, NULL, &concrete_tx.w, &concrete_tx.h);
    SDL_QueryTexture(floor_tx.texture, NULL, NULL, &floor_tx.w, &floor_tx.h);
    SDL_QueryTexture(shotgun_tx.texture, NULL, NULL, &shotgun_tx.w, &shotgun_tx.h);
    SDL_QueryTexture(mob_tx.texture, NULL, NULL, &mob_tx.w, &mob_tx.h);
    SDL_QueryTexture(mob_tx.texture, NULL, NULL, &barrel_tx.w, &barrel_tx.h);

    SDL_FreeSurface(wall_sf);
    SDL_FreeSurface(concrete_sf);
    SDL_FreeSurface(shotgun_sf);
    SDL_FreeSurface(floor_sf);
    SDL_FreeSurface(mob_sf);
    SDL_FreeSurface(barrel_sf);
}

int main (int argv, char * argc[]){
    int last_time, now;
    last_time = now = 0;
    int ms = 10000;
    cent_angle = player.angle + PLAYER_FOV / 2;

    initSDL();
    atexit(print_exit);

    SDL_ShowCursor(0);

    create_textures();
    CLEAR(gobjects);
    gobjects[0] = (GObject){500, 425, 0, &mob_tx, 0, 25, 25};
    gobjects[1] = (GObject){750, 170, 50,  &barrel_tx, 0, 25, 25};
    gobjects[2] = (GObject){700, 170, 0, &mob_tx, 0, 25, 25};

    gobjects_cnt = 3;

    for (int y=0; y < BOARD_HEIGHT; y++){
        for(int x=0; x < BOARD_WIDTH; x++){
            switch (board_draw[y][x]) {
                case '#':
                board[y][x] = (Block) {CONCRETE};
                break;
                case '0':
                board[y][x] = (Block) {BRICK};
                break;
                case ' ':
                board[y][x] = (Block) {EMPTY};
                break;
            }
        }
    }

    for (int i = 0; i < player.rays_count; i++){
        player.rays[i] = (Ray){0};
    } 

    while (true) {
        if ((now = clock())/ms - last_time/ms >= 100.0 / FPS){ // <--- limit fps
            last_time = now;

            prepareScene();

            doInput();
            input_handler();
            presentScene();
        }
    }

    return 0;
}