#include <complex.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>

#define FPS 30
#define MS 10000
#define ESCAPE 27
#define KEY_W 119
#define KEY_S 115
#define KEY_A 97
#define KEY_D 100
#define PI 3.141592654
#define BLOCK_SIZE 50
#define BOOST_SPEED 0.8
#define STOP_SPEED 1
#define ROTATION_SPEED 3.5
#define PLAYER_FOV 70
#define BOARD_HEIGHT 20
#define BOARD_WIDTH 20

#define COLOR_FULLBLACK 9
#define COLOR_GRAY 10

#define BRIGHT_CHARS "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. "

typedef struct {
    int length;
} Ray;

typedef struct {
    float x;
    float y;
    int FOV;
    float angle;
    int rays_count;
    float speed;
    Ray rays[PLAYER_FOV];
    
} Player;

typedef enum {
    CONCRETE,
    BRICK,
    EMPTY
} BlockType;

typedef struct {
    BlockType block_type;
} Block;

int DISP_WIDTH, DISP_HEIGHT;

char board_draw[BOARD_HEIGHT][BOARD_WIDTH + 1] = \
    {
        "####################",
        "#                  #",
        "####               #",
        "#            #     #",
        "####       ##      #",
        "#  #               #",
        "#  # 00      0     #",
        "#            #     #",
        "#          ##0     #",
        "#      0           #",
        "####   0           #",
        "#            #     #",
        "####      ####     #",
        "#  #               #",
        "#  # 00            #",
        "#             #    #",
        "#             0    #",
        "########0###########",
    };

char scope[3][6] = {
    "|",
    "==+==",
    "|"
};

double degToRad(float x){
    return (double)((x * PI) / 180);
}

void getrayxy(int length, float angle, float *x, float *y){
    int real_angle;
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

Block board[BOARD_HEIGHT][BOARD_WIDTH];

void normal_clear(){
    for (int dy = 0; dy < DISP_HEIGHT; dy++){
        for (int dx = 0; dx < DISP_WIDTH; dx++){
            mvprintw(dy, dx, " ");  
        }
    }
    bkgd(COLOR_PAIR(1));
}


int main(int argc, char *argv[]){
    initscr();
    noecho();
    nodelay(stdscr, true);
    cbreak();
    curs_set(false);
    keypad(stdscr, true);
    start_color();
    init_color(COLOR_FULLBLACK, 0, 0, 0);
    init_color(COLOR_GRAY, 300, 300, 300);
    init_color(11, 100, 100, 100);
    init_color(12, 550, 0, 0);
    init_pair(1, COLOR_GRAY, COLOR_FULLBLACK);
    init_pair(2, COLOR_RED, COLOR_FULLBLACK);
    init_pair(3, COLOR_GREEN, COLOR_FULLBLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLUE);

    getmaxyx(stdscr, DISP_HEIGHT, DISP_WIDTH);
    int now, last_time = 0;
    float ray_angle;
    int keydown;
    int lol = 90;
    float new_x, new_y = 0;
    float angle_x, angle_y = 0;
    float inertia_angle = 0;
    float wobbly = 0;
    float ray_x, ray_y;
    bool wobbly_up = true;
    Block *cur_block;

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

    Player player = {116, 180, PLAYER_FOV, 90,  PLAYER_FOV, 0};

    for (int i = 0; i < PLAYER_FOV; i++){
        player.rays[i] = (Ray){0};
    }   

    int pixel_size = (int) (DISP_WIDTH / player.rays_count);
    char pixel_line[pixel_size];
    int inp_err_counter = 0;
    int wall_h;
    MEVENT mouseevent;

    getch();

    while (true){
        if ((now = clock())/MS - last_time/MS >= 100 / FPS){
            last_time = now;

            if (player.angle >= 360){
                player.angle = 1;
            } else if (player.angle < 0){
                player.angle = 359;
            }

            int cent_angle = player.angle + player.rays_count / 2;

            mvprintw(0, 0, "%d", cent_angle);
            mvprintw(3, 0, "x: %f y: %f", player.x, player.y);
            mvprintw(4, 0, "newx: %f newy: %f", new_x, new_y);

            for (int y=0; y < BOARD_HEIGHT; y++){
                mvprintw(y, DISP_WIDTH - 20, "%s", board_draw[y]);
            }

            mvprintw((int)(player.y/BLOCK_SIZE), (int)(player.x/BLOCK_SIZE) + DISP_WIDTH - 20, "@");
            getrayxy(60, cent_angle, &angle_x, &angle_y);
            angle_x += player.x;
            angle_y += player.y;
            mvprintw((int)(angle_y/BLOCK_SIZE), (int)(angle_x/BLOCK_SIZE) + DISP_WIDTH - 20, ">");
            angle_x = angle_y = 0;
            new_x = new_y = 0.0;

            keydown = getch();
            mvprintw(3, 40, "keydown: %d", keydown);
            if (keydown == ESCAPE){
                break;
            } else if (keydown == KEY_LEFT){
                player.angle -= ROTATION_SPEED;
            } else if (keydown == KEY_RIGHT){
                player.angle += ROTATION_SPEED;
            } else {
                if (keydown == KEY_S || keydown == KEY_W ||keydown == KEY_D ||keydown == KEY_A){
                    player.speed += BOOST_SPEED;
                    if (player.speed > 6){
                        player.speed = 6;
                    }

                    switch (keydown) {
                        case KEY_S:
                        inertia_angle = cent_angle - 180;
                        break;
                        case KEY_W:
                        inertia_angle = cent_angle;
                        break;
                        case KEY_D:
                        inertia_angle = cent_angle + 90;
                        break;
                        case KEY_A:
                        inertia_angle = cent_angle - 90;
                        break;
                    }
                } 
            }

            getrayxy(player.speed, inertia_angle, &new_x, &new_y);

            if (board[(int)((player.y + new_y)/BLOCK_SIZE)][(int)((player.x + new_x)/BLOCK_SIZE)].block_type == EMPTY){
                player.x += new_x;
                player.y += new_y;
            }
            
            normal_clear();

            if (keydown == ERR) inp_err_counter++;

            if (inp_err_counter >= 2){
                inp_err_counter = 0;
                player.speed -= STOP_SPEED;
                if (player.speed < 0){
                    player.speed = 0;
                } 
            }

            for(int i=0; i < player.FOV; i++){
                ray_angle = player.angle + player.FOV / player.rays_count * (i+1);
                if (ray_angle > 360) ray_angle = ray_angle - 360;

                while (player.rays[i].length <= 2000) {
                    getrayxy((double)player.rays[i].length, ray_angle, &ray_x, &ray_y);
                    ray_x = ray_x + player.x;
                    ray_y = ray_y + player.y;

                    if (ray_x >= BOARD_WIDTH * BLOCK_SIZE || ray_x < 0 || ray_y >= BOARD_HEIGHT * BLOCK_SIZE || ray_y < 0){
                        player.rays[i].length = 0;
                        break;
                    }

                    cur_block = &board[(int)(ray_y/BLOCK_SIZE)][(int)(ray_x/BLOCK_SIZE)];
                    if (cur_block->block_type != EMPTY){
                        wall_h = (int) (DISP_HEIGHT / (player.rays[i].length *(cos(degToRad(ray_angle - cent_angle)))) * 40);
                        // if (wall_h < 4){
                        //     wall_h = 4;
                        // }
                        if (ray_y - (int)(ray_y / BLOCK_SIZE) * BLOCK_SIZE < 20){
                            wall_h = 0;
                        } 

                        for (int y=0; y < wall_h; y++){
                            attron(COLOR_PAIR(cur_block->block_type + 1));
                            mvprintw(DISP_HEIGHT/2 + wall_h/2 - y +  wobbly, i * 2, "%c", BRIGHT_CHARS[(int)(player.rays[i].length / 400.0 * strlen(BRIGHT_CHARS))]);
                            attroff(cur_block->block_type + 1);
                        }
                        player.rays[i].length = 0;
                        break;
                    }
                    player.rays[i].length++;
                }
            }
            for (int y=0; y < 3; y++){
                attron(COLOR_PAIR(3));
                mvprintw(DISP_HEIGHT / 2 + y, player.rays_count / 2 - strlen(scope[y])/2, "%s", scope[y]);
                attroff(COLOR_PAIR(3));
            }
        }
    }
    endwin();
    printf("%d\n", keydown);
}