#include "lcd_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usart.h"
#include "esp_log.h"

static const char *TAG = "LCD_DISPLAY";
static char g_display_data[UART_BUF_SIZE] = {0};  // 显示数据缓冲区

/**
 * @brief       初始化LCD显示
 * @param       无
 * @retval      无
 */
void lcd_display_init(void) {
    lcd_cfg_t lcd_config_info = {0};
    lcd_config_info.notify_flush_ready = NULL;
    lcd_init(lcd_config_info);          /* 初始化LCD */
    
    //初始化显示内容
    lcd_clear(WHITE);
    lcd_show_string(10, 10, 240, 32, 32, "UART STM32-ESP32", RED);
    lcd_show_string(10, 60, 240, 20, 16, "-------------------", BLUE);
    lcd_show_string(10, 80, 240, 16, 24, "RX:", BLACK);
    
    ESP_LOGI(TAG, "LCD_init OK");
}

/**
 * @brief       LCD显示任务
 * @param       pvParameters: 传入参数
 * @retval      无
 */
static void lcd_display_task(void *pvParameters) {
    while (1) {
        // 获取最新接收的数据
        uart_get_received_data(g_display_data, UART_BUF_SIZE);
        
        // 更新显示
        lcd_show_string(10, 120, 250, 24, 24, g_display_data, BLACK);
        
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms刷新一次
    }
}

/**
 * @brief       创建LCD显示任务
 * @param       无
 * @retval      无
 */
void lcd_create_display_task(void) {
    xTaskCreate(lcd_display_task, "lcd_display_task", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "LCD显示任务创建成功");
}

/**
 * @brief       更新LCD显示内容
 * @param       data: 要显示的数据
 * @retval      无
 */
void lcd_update_display(const char* data) {
    if (data == NULL) return;
    lcd_show_string(10, 150, 250, 24, 24, data, BLACK);
}
    