/**
 ****************************************************************************************************
 * @file        lvgl_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2024-07-26
 * @brief       lvgl demo
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
 
#ifndef __LVGL_DEMO_H
#define __LVGL_DEMO_H

#include "lcd.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdmmc_cmd.h"
#include "spi_sd.h"
#include "lv_camera_ui.h"
#include "touch.h"
#include "esp_dma_utils.h"
#include "lv_start_ui.h"
#include "lv_main_ui.h"
#include "key.h"


extern lv_disp_drv_t disp_drv;      /* 回调函数的参数 */

/* 函数声明 */
void lvgl_demo(void);
void lvgl_mux_unlock(void);
bool lvgl_mux_lock(int timeout_ms);
void touchpad_get_xy(lv_coord_t *x, lv_coord_t *y);
#endif
