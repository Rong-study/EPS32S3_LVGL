/**
 ****************************************************************************************************
 * @file        lv_start_ui.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-26
 * @brief       开机Ui
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32S3 BOX 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "lv_start_ui.h"


/* 开机界面 */
#define PI  (3.14159f)

/* 声明ATK LOGO */
LV_IMG_DECLARE(esp_logo)

/* 开机界面所需的控件 */
typedef struct {
    lv_obj_t *logo_box;
    lv_obj_t *logo_obj;
    lv_obj_t *logo_arc[3];
    int count_val;
}lv_starting_obj_t;
/* 声明开机控件结构体 */
static lv_starting_obj_t my_start_obj;

static lv_color_t arc_color[] = {
    LV_COLOR_MAKE(232, 87, 116),
    LV_COLOR_MAKE(126, 87, 162),
    LV_COLOR_MAKE(90, 202, 228),
};


/**
 * @brief       定时器回调函数
 * @param       timer：定时器句柄
 * @retval      无
 */
static void lv_anim_timer_cb(lv_timer_t *timer)
{
    lv_starting_obj_t *timer_ctx = (lv_starting_obj_t *) timer->user_data;
    int count = timer_ctx->count_val;

    /* 开启动画 */
    if (count < 360) 
    {
        lv_coord_t arc_start = count > 0 ? (1 - cosf(count / 180.0f * PI)) * 270 : 0;
        lv_coord_t arc_len = (sinf(count / 180.0f * PI) + 1) * 135;

        for (size_t i = 0; i < sizeof(timer_ctx->logo_arc) / sizeof(timer_ctx->logo_arc[0]); i++)
        {
            lv_arc_set_bg_angles(timer_ctx->logo_arc[i], arc_start, arc_len);
            lv_arc_set_rotation(timer_ctx->logo_arc[i], (count + 120 * (i + 1)) % 360);
        }
    }

    /* 结束动画并删除开启界面 */
    if ((count += 5) == 220)
    {
        lv_timer_del(timer);
        lv_obj_del(timer_ctx->logo_box);
        lv_mian_ui();
    }
    else
    {
        timer_ctx->count_val = count;
    }
}

/**
 * @brief       在logo中设置圆弧
 * @param       scr：父类对象
 * @retval      无
 */
static void lv_logo_start_animation(lv_obj_t *scr)
{
    /* logo中间对齐 */
    lv_obj_center(my_start_obj.logo_obj);

    /* 创建圆弧 */
    for (size_t i = 0; i < sizeof(my_start_obj.logo_arc) / sizeof(my_start_obj.logo_arc[0]); i++) 
    {
        my_start_obj.logo_arc[i] = lv_arc_create(scr);

        /* 设置圆弧参数 */
        lv_obj_set_size(my_start_obj.logo_arc[i], 220 - 30 * i, 220 - 30 * i);
        lv_arc_set_bg_angles(my_start_obj.logo_arc[i], 120 * i, 10 + 120 * i);
        lv_arc_set_value(my_start_obj.logo_arc[i], 0);

        /* 设置圆弧样式 */
        lv_obj_remove_style(my_start_obj.logo_arc[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(my_start_obj.logo_arc[i], 10, 0);
        lv_obj_set_style_arc_color(my_start_obj.logo_arc[i], arc_color[i], 0);

        /* 对齐圆弧 */
        lv_obj_center(my_start_obj.logo_arc[i]);
        lv_obj_clear_flag(my_start_obj.logo_arc[i],LV_OBJ_FLAG_CLICKABLE);
    }

    /* 设置定时数值 */
    my_start_obj.count_val = -90;
    lv_timer_create(lv_anim_timer_cb, 40, &my_start_obj);

}

/**
 * @brief       logo 界面
 * @param       无
 * @retval      无
 */
void lv_start_ui(void)
{
    my_start_obj.logo_box = lv_obj_create(lv_scr_act());
    lv_obj_set_size(my_start_obj.logo_box,lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    lv_obj_center(my_start_obj.logo_box);
    lv_obj_clear_flag(my_start_obj.logo_box,LV_OBJ_FLAG_SCROLLABLE);
    my_start_obj.logo_obj = lv_img_create(my_start_obj.logo_box);
    lv_img_set_src( my_start_obj.logo_obj, &esp_logo);
    lv_logo_start_animation(my_start_obj.logo_box);
}