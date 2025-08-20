/**
 ****************************************************************************************************
 * @file        lvgl_demo.c
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

#include "lvgl_demo.h"

static const char *TAG = "atk_lvgl";

#define EXAMPLE_LVGL_TICK_PERIOD_MS     2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS  500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS  1
#define EXAMPLE_LVGL_TASK_STACK_SIZE    (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY      2
lv_indev_t *indev_keypad = NULL;        /* 按键组 */
uint32_t back_act_key = 0;              /* 返回主界面按键 */
uint32_t g_last_key = 0;                /* 记录前一个按键值 */
extern lv_obj_t * back_btn;
extern uint8_t temp[4];

/* 互斥锁 */
static SemaphoreHandle_t lvgl_mux = NULL;

/**
 * @brief       lvgl刷新回调函数
 * @param       drv         :lcd设备
 * @param       area        :绘画区域
 * @param       color_map   :绘画颜色
 * @retval      无
 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t) drv->user_data, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

/**
 * @brief       提供LVGL节拍
 * @param       arg         :转入参数（未使用）
 * @retval      无
 */
static void lvgl_increase_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

/**
 * @brief       进入互斥锁
 * @param       timeout_ms         :等待时间
 * @retval      
 */
bool lvgl_mux_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

/**
 * @brief       释放互斥锁
 * @param       无
 * @retval      无
 */
void lvgl_mux_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

/**
 * @brief       释放互斥锁
 * @param       无
 * @retval      无
 */
static void lvgl_port_task(void *arg)
{
    arg = arg;
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;

    while (1)
    {
        /* 锁定互斥锁,因为LVGL api不是线程安全的*/
        if (lvgl_mux_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            /* 使用互斥锁 */
            lvgl_mux_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

/**
 * @brief       获取触摸屏状态
 * @param       无
 * @retval      true:有触摸，false:无触摸
 */
static bool touchpad_is_pressed(void)
{
    tp_dev.scan(0);

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        return true;
    }

    return false;
}

/**
 * @brief       读取坐标
 * @param       x:x轴坐标
 * @param       y:y轴坐标
 * @retval      无
 */
void touchpad_get_xy(lv_coord_t *x, lv_coord_t *y)
{
    (*x) = tp_dev.x[0];
    (*y) = tp_dev.y[0];
}

/**
 * @brief       坐标读取
 * @param       indev_drv:输入设备句柄
 * @param       data:输入设备数据存储
 * @retval      无
 */
void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    if(touchpad_is_pressed())
    {
        touchpad_get_xy(&last_x, &last_y);  
        data->state = LV_INDEV_STATE_PR;
    } 
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /* 设置坐标 */
    data->point.x = last_x;
    data->point.y = last_y;
}

static uint32_t keypad_get_key(void);
/**
 * @brief       图形库的键盘读取回调函数
 * @param       indev_drv : 键盘设备
 * @arg         data      : 输入设备数据结构体
 * @retval      无
 */
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    /* 获取按键是否被按下，并保存键值 */
    uint32_t act_key = keypad_get_key();

    if(act_key != 0) 
    {
        data->state = LV_INDEV_STATE_PR;

        /* 将键值转换成 LVGL 的控制字符 */
        switch(act_key)
        {
            case KEY0_PRES:
                act_key = LV_KEY_NEXT;
            break;

            case BOOT_PRES:
                act_key = BOOT_PRES;
                back_act_key = BOOT_PRES;
                lv_event_send(back_btn,LV_EVENT_CLICKED,NULL);
            
            break;
            
            case KEY1_PRES:
                act_key = LV_KEY_ENTER;
            break;
        }
        g_last_key = act_key;
    }
    else 
    {
        data->state = LV_INDEV_STATE_REL;
        g_last_key = 0;
    }

    data->key = g_last_key;
}

/**
 * @brief       通过IO扩展芯片获取当前正在按下的按键
 * @param       无
 * @retval      0 : 按键没有被按下
 */
static uint32_t keypad_get_key(void)
{
    uint8_t key1;
    uint8_t key2;

    key2 = key_scan(0);

    if (BOOT_PRES == key2)
    {
        return key2;
    } 
    else 
    {
        key1 = xl9555_key_scan(0);

        if (KEY0_PRES == key1 || KEY1_PRES == key1)
        {
            return key1;
        } 
    }

    return 0;
}


/**
 * @brief       初始化触摸设备
 * @param       无
 * @retval      无
 */
void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv; 
    tp_dev.init();

    if (temp[0] == 0x5)
    {
        /* 注册触摸输入设备 */
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = touchpad_read;
        lv_indev_drv_register(&indev_drv);
    }
    else
    {
        /* 注册键盘输入设备 */
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv.read_cb = keypad_read;
        indev_keypad = lv_indev_drv_register(&indev_drv);
    }

}

lv_disp_drv_t disp_drv;      /* 回调函数的参数 */

/**
 * @brief       lvgl程序入口
 * @param       无
 * @retval      无
 */
void lvgl_demo(void)
{
    static lv_disp_draw_buf_t disp_buf; /* 绘画区域的存储区 */
       
    if (sd_spi_init() == ESP_OK)        /* 初始化TF卡 */
    {
        lv_smail_icon_add_state(TF_STATE);
    }
    else
    {
        lv_smail_icon_clear_state(TF_STATE);
    }

    if (usb_camera_init() == ESP_OK)
    {
        ESP_LOGI(TAG, "usb_camera fail");
    }

    lv_init();                          /* 初始化lvgl */

    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;

    ESP_ERROR_CHECK(esp_dma_malloc(lcd_dev.width * 100 * sizeof(lv_color_t), ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&buf1, NULL));
    ESP_ERROR_CHECK(esp_dma_malloc(lcd_dev.width * 100 * sizeof(lv_color_t), ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&buf2, NULL));
    assert(buf1);
    assert(buf2);
    /* 输出内存地址 */
    ESP_LOGI(TAG, "buf1@%p, buf2@%p", buf1, buf2);
    /* 初始化绘画存储区 */
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, lcd_dev.width * 100);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = lcd_dev.width;
    disp_drv.ver_res = lcd_dev.height;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    /* 显示设备注册 */
    lv_disp_drv_register(&disp_drv);
    /* 触摸设备注册 */
    lv_port_indev_init();

    /* 创建定时器提供lvgl时钟节拍 */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_increase_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);

    xTaskCreate(lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    /* 锁定互斥锁，因为LVGL api不是线程安全的 */
    if (lvgl_mux_lock(-1))
    {
        //lv_test_ui();
        lv_mian_ui();
        /* 释放互斥锁 */
        lvgl_mux_unlock();
    }
}
