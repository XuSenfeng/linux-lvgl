#include "ui_snake_ai.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

///////////////////// VARIABLES ////////////////////

#define SNAKE_GRID_W 40
#define SNAKE_GRID_H 30
#define SNAKE_MAX_LEN (SNAKE_GRID_W * SNAKE_GRID_H)
#define SNAKE_CELL_SIZE 16

typedef struct {
    int x;
    int y;
} snake_point_t;

typedef enum {
    SNAKE_DIR_RIGHT = 0,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_UP,
    SNAKE_DIR_DOWN
} snake_dir_t;

static lv_obj_t *s_snake_page = NULL;
static lv_obj_t *s_board = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_score_label = NULL;
static lv_obj_t *s_ai_label = NULL;
static lv_obj_t *s_start_btn = NULL;
static lv_obj_t *s_btn_label = NULL;
static lv_timer_t *s_timer = NULL;
static lv_obj_t *s_cells[SNAKE_GRID_H][SNAKE_GRID_W];

static const char *s_wall_map[SNAKE_GRID_H] = {
    "1111111111111111111111111111111111111111",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1000000000000000000000000000000000000001",
    "1111111111111111111111111111111111111111"
};

static snake_point_t s_snake[SNAKE_MAX_LEN];
static int s_snake_len = 0;
static snake_point_t s_berry;
static snake_dir_t s_dir = SNAKE_DIR_RIGHT;
static int s_score = 0;
static uint32_t s_step = 0;
static bool s_running = false;

static int s_pipe_in_fd = -1;
static int s_pipe_out_fd = -1;
static char s_pipe_buf[128];
static size_t s_pipe_buf_len = 0;
static float s_ai_out[3] = {0.0f, 0.0f, 0.0f};
static bool s_ai_ready = false;


///////////////////// ANIMATIONS ////////////////////
static void ui_event_gesture(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if(event_code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}


///////////////////// FUNCTIONS ////////////////////

static void snake_pipe_try_create(void)
{
    if (access("/tmp/snake_input_pipe", F_OK) != 0) {
        mkfifo("/tmp/snake_input_pipe", 0666);
    }
    if (access("/tmp/snake_output_pipe", F_OK) != 0) {
        mkfifo("/tmp/snake_output_pipe", 0666);
    }
}

static void snake_pipe_try_open(void)
{
    if (s_pipe_in_fd < 0) {
        s_pipe_in_fd = open("/tmp/snake_input_pipe", O_WRONLY | O_NONBLOCK);
    }
    if (s_pipe_out_fd < 0) {
        s_pipe_out_fd = open("/tmp/snake_output_pipe", O_RDONLY | O_NONBLOCK);
    }
}

static void snake_pipe_close(void)
{
    if (s_pipe_in_fd >= 0) {
        close(s_pipe_in_fd);
        s_pipe_in_fd = -1;
    }
    if (s_pipe_out_fd >= 0) {
        close(s_pipe_out_fd);
        s_pipe_out_fd = -1;
    }
    s_pipe_buf_len = 0;
}

static void snake_pipe_write_state(const int state[20])
{
    if (s_pipe_in_fd < 0) {
        return;
    }

    char line[128];
    int len = snprintf(line, sizeof(line),
        "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        state[0], state[1], state[2], state[3], state[4],
        state[5], state[6], state[7], state[8], state[9],
        state[10], state[11], state[12], state[13], state[14],
        state[15], state[16], state[17], state[18], state[19]);
    printf("Writing state to pipe: %s", line);
    if (len <= 0) {
        return;
    }
    ssize_t ret = write(s_pipe_in_fd, line, (size_t)len);
    if (ret < 0 && errno == EPIPE) {
        snake_pipe_close();
    }
}

static void snake_pipe_read_output(void)
{
    if (s_pipe_out_fd < 0) {
        return;
    }

    char buf[128];
    ssize_t n = read(s_pipe_out_fd, buf, sizeof(buf));
    while (n <= 0) {
        n = read(s_pipe_out_fd, buf, sizeof(buf));
        sleep(0.01);
    }

    if ((size_t)n + s_pipe_buf_len >= sizeof(s_pipe_buf)) {
        s_pipe_buf_len = 0;
    }
    printf("Read %zd bytes from pipe: %.*s", n, (int)n, buf);

    memcpy(s_pipe_buf + s_pipe_buf_len, buf, (size_t)n);
    s_pipe_buf_len += (size_t)n;

    for (size_t i = 0; i < s_pipe_buf_len; i++) {
        if (s_pipe_buf[i] == '\n') {
            s_pipe_buf[i] = '\0';
            float a = 0.0f, b = 0.0f, c = 0.0f;
            if (sscanf(s_pipe_buf, "%f %f %f", &a, &b, &c) == 3) {
                s_ai_out[0] = a;
                s_ai_out[1] = b;
                s_ai_out[2] = c;
                s_ai_ready = true;
            }
            size_t remaining = s_pipe_buf_len - (i + 1);
            memmove(s_pipe_buf, s_pipe_buf + i + 1, remaining);
            s_pipe_buf_len = remaining;
            break;
        }
    }
}
static void snake_place_berry(void);
static void snake_reset(void)
{
    s_snake_len = 2;
    s_snake[0] = (snake_point_t){SNAKE_GRID_W / 2, SNAKE_GRID_H / 2};
    s_snake[1] = (snake_point_t){SNAKE_GRID_W / 2 - 1, SNAKE_GRID_H / 2};
    s_dir = SNAKE_DIR_RIGHT;
    s_score = 0;
    s_step = 0;

    snake_place_berry();
    s_ai_ready = false;
}

static bool snake_point_eq(snake_point_t a, snake_point_t b)
{
    return (a.x == b.x) && (a.y == b.y);
}

static bool snake_hit_body(snake_point_t p)
{
    for (int i = 1; i < s_snake_len; i++) {
        if (snake_point_eq(s_snake[i], p)) {
            return true;
        }
    }
    return false;
}

static bool snake_hit_wall(snake_point_t p)
{
    /* 越界或落在外圈(0 / width-1 / 0 / height-1)即视为撞墙，不依赖字符串越界 */
    if (p.x <= 0 || p.y <= 0 || p.x >= (SNAKE_GRID_W - 1) || p.y >= (SNAKE_GRID_H - 1)) {
        return true;
    }
    return s_wall_map[p.y][p.x] == '1';
}

static bool snake_is_wall_cell(int x, int y)
{
    if (x <= 0 || y <= 0 || x >= (SNAKE_GRID_W - 1) || y >= (SNAKE_GRID_H - 1)) {
        return true;
    }
    return s_wall_map[y][x] == '1';
}

static void snake_place_berry(void)
{
    static int temp = 0;
    snake_point_t p;
    for (int i = 0; i < 50; i++) {
            // p.x = 10;
            // p.y = 10;
            // 10, 10
        p.x =  1 + (rand() % (SNAKE_GRID_W - 2));
        p.y =  1 + (rand() % (SNAKE_GRID_H - 2));
        bool hit = false;
        if (snake_is_wall_cell(p.x, p.y)) {
            hit = true;
        }
        for (int j = 0; j < s_snake_len; j++) {
            if (snake_point_eq(s_snake[j], p)) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            s_berry = p;
            return;
        }
    }
}

static void snake_get_state(int state[20])
{
    snake_point_t head = s_snake[0];
    snake_point_t point_l = (snake_point_t){head.x - 1, head.y};
    snake_point_t point_r = (snake_point_t){head.x + 1, head.y};
    snake_point_t point_u = (snake_point_t){head.x, head.y - 1};
    snake_point_t point_d = (snake_point_t){head.x, head.y + 1};

    int danger_1b_r = snake_hit_body(point_r);
    int danger_1b_l = snake_hit_body(point_l);
    int danger_1b_u = snake_hit_body(point_u);
    int danger_1b_d = snake_hit_body(point_d);
    int danger_1w_r = snake_hit_wall(point_r);
    int danger_1w_l = snake_hit_wall(point_l);
    int danger_1w_u = snake_hit_wall(point_u);
    int danger_1w_d = snake_hit_wall(point_d);

    int danger_b_l = 0, danger_b_r = 0, danger_b_u = 0, danger_b_d = 0;
    for (int i = 1; i < head.x; i++) {
        if (snake_hit_body((snake_point_t){i, head.y})) {
            danger_b_l = 1;
            break;
        }
    }
    for (int i = head.x + 1; i < (SNAKE_GRID_W - 2); i++) {
        if (snake_hit_body((snake_point_t){i, head.y})) {
            danger_b_r = 1;
            break;
        }
    }
    for (int i = 1; i < head.y; i++) {
        if (snake_hit_body((snake_point_t){head.x, i})) {
            danger_b_u = 1;
            break;
        }
    }
    for (int i = head.y + 1; i < (SNAKE_GRID_H - 2); i++) {
        if (snake_hit_body((snake_point_t){head.x, i})) {
            danger_b_d = 1;
            break;
        }
    }

    int dir_l = (s_dir == SNAKE_DIR_LEFT);
    int dir_r = (s_dir == SNAKE_DIR_RIGHT);
    int dir_u = (s_dir == SNAKE_DIR_UP);
    int dir_d = (s_dir == SNAKE_DIR_DOWN);
    int berry_l = (s_berry.x < head.x);
    int berry_r = (s_berry.x > head.x);
    int berry_u = (s_berry.y < head.y);
    int berry_d = (s_berry.y > head.y);

    int tmp[20] = {
        danger_1b_r, danger_1b_l, danger_1b_u, danger_1b_d,
        danger_1w_r, danger_1w_l, danger_1w_u, danger_1w_d,
        danger_b_l, danger_b_r, danger_b_u, danger_b_d,
        dir_l, dir_r, dir_u, dir_d,
        berry_l, berry_r, berry_u, berry_d
    };
    for (int i = 0; i < 20; i++) {
        state[i] = tmp[i];
    }
}

static int snake_ai_get_action(void)
{
    int action = 0;
    float max = s_ai_out[0];
    if (s_ai_out[1] > max) {
        max = s_ai_out[1];
        action = 1;
    }
    if (s_ai_out[2] > max) {
        action = 2;
    }
    return action;
}

static void snake_apply_action(int action)
{
    static const snake_dir_t clockwise[4] = {
        SNAKE_DIR_RIGHT, SNAKE_DIR_DOWN, SNAKE_DIR_LEFT, SNAKE_DIR_UP
    };
    int idx = 0;
    for (int i = 0; i < 4; i++) {
        if (clockwise[i] == s_dir) {
            idx = i;
            break;
        }
    }
    if (action == 1) {
        idx = (idx + 1) % 4;
    } else if (action == 2) {
        idx = (idx + 3) % 4;
    }
    s_dir = clockwise[idx];
}

static snake_point_t snake_move_forward(void)
{
    snake_point_t head = s_snake[0];
    snake_point_t new_head = head;
    if (s_dir == SNAKE_DIR_RIGHT) {
        new_head.x += 1;
    } else if (s_dir == SNAKE_DIR_LEFT) {
        new_head.x -= 1;
    } else if (s_dir == SNAKE_DIR_UP) {
        new_head.y -= 1;
    } else {
        new_head.y += 1;
    }
    printf("New head position: (%d, %d)\n", new_head.x, new_head.y);
    return new_head;
}

static void snake_step(int action)
{
    s_step++;
    snake_apply_action(action);
    snake_point_t new_head = snake_move_forward();

    /* 按长度-1平移身体，尾巴被覆盖相当于 pop tail */
    for (int i = s_snake_len - 1; i > 0; i--) {
        s_snake[i] = s_snake[i - 1];
    }
    s_snake[0] = new_head;

    if (snake_point_eq(new_head, s_berry)) {
        s_snake_len++;
        if (s_snake_len > SNAKE_MAX_LEN) {
            s_snake_len = SNAKE_MAX_LEN;
        }
        s_score++;
        snake_place_berry();
    }

    if (snake_hit_wall(new_head) || snake_hit_body(new_head) || s_step > (uint32_t)(100 * s_snake_len)) {
        lv_label_set_text(s_status_label, "Game Over");
        snake_reset();
        return;
    }
}

static void snake_draw_board(void)
{
    for (int y = 0; y < SNAKE_GRID_H; y++) {
        for (int x = 0; x < SNAKE_GRID_W; x++) {
            if (snake_is_wall_cell(x, y)) {
                lv_obj_set_style_bg_color(s_cells[y][x], lv_color_hex(0xBFC9CA), 0);
            } else {
                lv_obj_set_style_bg_color(s_cells[y][x], lv_color_hex(0xF1F2F6), 0);
            }
        }
    }

    lv_obj_set_style_bg_color(s_cells[s_berry.y][s_berry.x], lv_color_hex(0xE74C3C), 0);

    for (int i = s_snake_len - 1; i >= 0; i--) {
        snake_point_t p = s_snake[i];
        if (i == 0) {
            lv_obj_set_style_bg_color(s_cells[p.y][p.x], lv_color_hex(0x1E8449), 0);
        } else {
            lv_obj_set_style_bg_color(s_cells[p.y][p.x], lv_color_hex(0x27AE60), 0);
        }
    }
}

static void snake_update_labels(void)
{
    if (s_score_label) {
        lv_label_set_text_fmt(s_score_label, "得分 %d", s_score);
    }
    if (s_ai_label) {
        lv_label_set_text_fmt(s_ai_label, "AI output %.3f %.3f %.3f", (double)s_ai_out[0], (double)s_ai_out[1], (double)s_ai_out[2]);
    }
}

static void snake_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    snake_pipe_try_open();

    if (!s_running) {
        snake_draw_board();
        snake_update_labels();
        return;
    }

    int state[20] = {0};
    snake_get_state(state);
    snake_pipe_write_state(state);
    sleep(0.02);  // Give AI some time to respond
    snake_pipe_read_output();

    /* 若尚未收到一次 AI 输出，先等一帧，避免用初始 0 造成错误转向 */
    if (!s_ai_ready) {
        snake_draw_board();
        snake_update_labels();
        return;
    }

    int action = snake_ai_get_action();
    snake_step(action);
    snake_draw_board();
    snake_update_labels();
}

static void snake_start_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    s_running = !s_running;
    if (s_running) {
        lv_label_set_text(s_status_label, "AI running");
        lv_label_set_text(s_btn_label, "Pause");
    } else {
        lv_label_set_text(s_status_label, "Paused");
        lv_label_set_text(s_btn_label, "Start");
    }
}


///////////////////// SCREEN init ////////////////////

void ui_Snake_AI_init(void *arg)
{
    LV_UNUSED(arg);

    snake_pipe_try_create();

    s_snake_page = lv_obj_create(NULL);
    lv_obj_clear_flag(s_snake_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_snake_page, lv_color_hex(0xECF0F1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_snake_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    int32_t screen_w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t screen_h = lv_display_get_vertical_resolution(lv_display_get_default());

    lv_obj_t *card = lv_obj_create(s_snake_page);
    lv_obj_set_size(card, screen_w - 40, screen_h - 40);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x7F8C8D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(card, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(card, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "snake AI");
    // lv_obj_set_style_text_font(title, &tomato_font32, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2C3E50), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    s_status_label = lv_label_create(card);
    lv_label_set_text(s_status_label, "temp");
    // lv_obj_set_style_text_font(s_status_label, &ui_font_heiti22, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x7F8C8D), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 52);

    s_board = lv_obj_create(card);
    lv_obj_set_size(s_board, SNAKE_GRID_W * SNAKE_CELL_SIZE + 8, SNAKE_GRID_H * SNAKE_CELL_SIZE + 8);
    lv_obj_align(s_board, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_radius(s_board, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_board, lv_color_hex(0xF9F9F9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_board, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(s_board, 4, 0);
    lv_obj_clear_flag(s_board, LV_OBJ_FLAG_SCROLLABLE);

    for (int y = 0; y < SNAKE_GRID_H; y++) {
        for (int x = 0; x < SNAKE_GRID_W; x++) {
            lv_obj_t *cell = lv_obj_create(s_board);
            lv_obj_set_size(cell, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1);
            lv_obj_set_pos(cell, x * SNAKE_CELL_SIZE, y * SNAKE_CELL_SIZE);
            lv_obj_set_style_radius(cell, 4, 0);
            lv_obj_set_style_bg_color(cell, lv_color_hex(0xF1F2F6), 0);
            lv_obj_set_style_bg_opa(cell, 255, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            s_cells[y][x] = cell;
        }
    }

    s_score_label = lv_label_create(card);
    lv_label_set_text(s_score_label, "得分 0");
    lv_obj_set_style_text_font(s_score_label, &ui_font_heiti22, 0);
    lv_obj_set_style_text_color(s_score_label, lv_color_hex(0x2C3E50), 0);
    lv_obj_align(s_score_label, LV_ALIGN_BOTTOM_LEFT, 20, -24);

    s_ai_label = lv_label_create(card);
    lv_label_set_text(s_ai_label, "AI output 0.000 0.000 0.000");
    lv_obj_set_style_text_font(s_ai_label, &ui_font_heiti22, 0);
    lv_obj_set_style_text_color(s_ai_label, lv_color_hex(0x2C3E50), 0);
    lv_obj_align(s_ai_label, LV_ALIGN_BOTTOM_LEFT, 20, -56);

    s_start_btn = lv_button_create(card);
    lv_obj_set_size(s_start_btn, 160, 56);
    lv_obj_set_style_radius(s_start_btn, 28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_start_btn, lv_color_hex(0x27AE60), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_start_btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_start_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(s_start_btn, snake_start_btn_cb, LV_EVENT_CLICKED, NULL);

    s_btn_label = lv_label_create(s_start_btn);
    lv_label_set_text(s_btn_label, "Start");
    lv_obj_set_style_text_color(s_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_btn_label);

    snake_reset();
    snake_draw_board();
    snake_update_labels();

    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    s_timer = lv_timer_create(snake_timer_cb, 100, NULL);

    lv_obj_add_event_cb(s_snake_page, ui_event_gesture, LV_EVENT_GESTURE, NULL);

    // load page
    lv_scr_load_anim(s_snake_page, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_Snake_AI_deinit()
{
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    snake_pipe_close();
}


void ui_Snake_AI_button(lv_obj_t * ui_HomeScreen, lv_obj_t * ui_AppIconContainer, int x, int y, void (*button_cb)(lv_event_t *)){
    lv_obj_t * ui_TimerBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_TimerBtn, 70);
    lv_obj_set_height(ui_TimerBtn, 70);
    lv_obj_set_x(ui_TimerBtn, x);
    lv_obj_set_y(ui_TimerBtn, y);
    lv_obj_add_flag(ui_TimerBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(ui_TimerBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_TimerBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TimerBtn, lv_color_hex(0x11999E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TimerBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Icon2048 = lv_label_create(ui_TimerBtn);
    lv_obj_set_width(ui_Icon2048, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Icon2048, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_Icon2048, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Icon2048, "");
    lv_obj_set_style_text_color(ui_Icon2048, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Icon2048, 196, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Icon2048, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_add_event_cb(ui_TimerBtn, button_cb, LV_EVENT_CLICKED, "SnakeAIPage");
}