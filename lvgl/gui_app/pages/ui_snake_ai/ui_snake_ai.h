#ifndef _UI_SNAKE_H
#define _UI_SNAKE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../ui.h"

void ui_Snake_AI_init(void *arg);
void ui_Snake_AI_deinit(void);
void ui_Snake_AI_button(lv_obj_t * ui_HomeScreen, lv_obj_t * ui_AppIconContainer, int x, int y, void (*button_cb)(lv_event_t *));

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif