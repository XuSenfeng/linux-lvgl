#include "ui_TimerPage.h"

///////////////////// VARIABLES ////////////////////

static lv_obj_t *s_timer_page = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_timer_t *s_timer = NULL;
static lv_display_rotation_t s_prev_rotation = LV_DISPLAY_ROTATION_0;
static bool s_rotation_changed = false;


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

static void ui_timer_update_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!s_time_label) {
        return;
    }

    uint32_t total_sec = lv_tick_get() / 1000U;
    uint32_t sec = total_sec % 60U;
    uint32_t min = (total_sec / 60U) % 60U;
    uint32_t hour = (total_sec / 3600U) % 24U;

    static char time_buf[9];
    time_buf[0] = '0' + (hour / 10U);
    time_buf[1] = '0' + (hour % 10U);
    time_buf[2] = ':';
    time_buf[3] = '0' + (min / 10U);
    time_buf[4] = '0' + (min % 10U);
    time_buf[5] = ':';
    time_buf[6] = '0' + (sec / 10U);
    time_buf[7] = '0' + (sec % 10U);
    time_buf[8] = '\0';
    lv_label_set_text(s_time_label, time_buf);
}


///////////////////// SCREEN init ////////////////////

LV_FONT_DECLARE(font_led128);

void ui_TimerPage_init(void *arg)
{
    // LV_UNUSED(arg);
    printf("timer ...");
    s_timer_page = lv_obj_create(NULL);
    lv_obj_clear_flag(s_timer_page, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    // lv_display_t *disp = lv_display_get_default();
    // if (disp) {
    //     s_prev_rotation = lv_display_get_rotation(disp);
    //     lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    //     s_rotation_changed = true;
    // } else {
    //     s_rotation_changed = false;
    // }

    s_time_label = lv_label_create(s_timer_page);
    lv_obj_set_style_text_font(s_time_label, &font_led128, 0);
    lv_label_set_text(s_time_label, "00:00:00");
    lv_obj_center(s_time_label);
    lv_obj_update_layout(s_time_label);
    int32_t label_w = lv_obj_get_width(s_time_label);
    int32_t label_h = lv_obj_get_height(s_time_label);
    // 设置旋转的中心点为标签中心
    lv_obj_set_style_transform_pivot_x(s_time_label, label_w / 2, 0);
    lv_obj_set_style_transform_pivot_y(s_time_label, label_h / 2, 0);
    // 旋转90度
    lv_obj_set_style_transform_angle(s_time_label, 900, 0);

    

    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    s_timer = lv_timer_create(ui_timer_update_cb, 1000, NULL);
    ui_timer_update_cb(NULL);

    lv_obj_add_event_cb(s_timer_page, ui_event_gesture, LV_EVENT_GESTURE, NULL);
    // load page
    lv_scr_load_anim(s_timer_page, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_TimerPage_deinit()
{
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    // if (s_rotation_changed) {
    //     lv_display_t *disp = lv_display_get_default();
    //     if (disp) {
    //         lv_display_set_rotation(disp, s_prev_rotation);
    //     }
    //     s_rotation_changed = false;
    // }
}


void ui_timer_button(lv_obj_t * ui_HomeScreen, lv_obj_t * ui_AppIconContainer, int x, int y, void (*button_cb)(lv_event_t *)){
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
    
    lv_obj_add_event_cb(ui_TimerBtn, button_cb, LV_EVENT_CLICKED, "TimerPage");
}