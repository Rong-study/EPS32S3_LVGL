
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "led.h"
#include "lcd.h"
#include "string.h"
#include "usart.h"
#include "lcd_display.h"


// // 全局变量
// #define UART_BUF_SIZE      1024             // 缓冲区大小
// char g_rx_buf[UART_BUF_SIZE] = {0};        // 接收缓冲区
// char g_display_str[UART_BUF_SIZE] = {0};   // 显示缓冲区
// int16_t g_send_count = 0;                 // 发送计数器




void app_main(void) {
    esp_err_t ret;

    // 初始化NVS
    ret = nvs_flash_init();            
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 初始化外设
    led_init();                         /* 初始化LED */
    myiic_init();                       /* IIC初始化 */
    xl9555_init();                      /* 初始化按键 */
    

    lcd_display_init();                /* 初始化显示 */
    
    // 初始化UART
    uart_comm_init();
    
    // 创建通信和显示任务
    uart_create_receive_task();
    uart_create_send_task();
    lcd_create_display_task();
    
    // 主循环状态指示
    while (1) {
        LED_TOGGLE();                  // 闪烁LED表示系统运行
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

































// /**
//  * @brief       程序入口
//  * @param       无
//  * @retval      无
//  */
// void app_main(void) {
//     esp_err_t ret;

//     // 初始化NVS
//     ret = nvs_flash_init();            
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ESP_ERROR_CHECK(nvs_flash_init());
//     }

//     // 初始化外设
//     led_init();                         /* 初始化LED */
//     myiic_init();                       /* IIC初始化 */
//     xl9555_init();                      /* 初始化按键 */
    
//     // 初始化LCD
//     lcd_cfg_t lcd_config_info = {0};
//     lcd_config_info.notify_flush_ready = NULL;
//     lcd_init(lcd_config_info);          /* 初始化LCD */
    
//     // 初始化UART
//     usart_init(115200);
//     printf("UART_init finish...\r\n");
    
//     // 创建UART接收任务
//     xTaskCreate(uart_receive_task, "uart_receive_task", 4096, NULL, 5, NULL);

//     // 创建UART发送任务
//     xTaskCreate(uart_send_task, "uart_send_task", 4096, NULL, 5, NULL);
    
//     // 清屏并显示标题
//     lcd_clear(WHITE);
//     lcd_show_string(10, 10, 240, 32, 32, "STM32 ESP32", RED);

//     lcd_show_string(10, 50, 240, 20, 20, "-------------------", RED);
    
//     lcd_show_string(10, 80, 240, 16, 16, "RX Data:", BLACK);
    
//     while (1) {
//         // 显示接收到的数据
//         lcd_show_string(10, 100, 250, 24, 24, g_display_str, BLACK);
        
//         LED_TOGGLE();                  // 闪烁LED表示系统运行
//         vTaskDelay(pdMS_TO_TICKS(100)); // 100ms刷新一次
//     }
// }











// /**
//  * @brief       程序入口
//  * @param       无
//  * @retval      无
//  */
// void app_main(void)
// {
//     uint8_t x = 0;
//     esp_err_t ret;

//     ret = nvs_flash_init();             /* 初始化NVS */

//     lcd_cfg_t lcd_config_info = {0};
//     lcd_config_info.notify_flush_ready = NULL;
    
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
//     {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ESP_ERROR_CHECK(nvs_flash_init());
//     }

//     led_init();                         /* 初始化LED */
//     myiic_init();                       /* IIC初始化 */
//     xl9555_init();                      /* 初始化按键 */
//     lcd_init(lcd_config_info);          /* 初始化LCD */

//     while (1)
//     {
//         switch (x)
//         {
//             case 0:
//             {
//                 lcd_clear(WHITE);
//                 break;
//             }
//             case 1:
//             {
//                 lcd_clear(BLACK);
//                 break;
//             }
//             case 2:
//             {
//                 lcd_clear(BLUE);
//                 break;
//             }
//             case 3:
//             {
//                 lcd_clear(RED);
//                 break;
//             }
//             case 4:
//             {
//                 lcd_clear(MAGENTA);
//                 break;
//             }
//             case 5:
//             {
//                 lcd_clear(GREEN);
//                 break;
//             }
//             case 6:
//             {
//                 lcd_clear(CYAN);
//                 break;
//             }
//             case 7:
//             {
//                 lcd_clear(YELLOW);
//                 break;
//             }
//             case 8:
//             {
//                 lcd_clear(BRRED);
//                 break;
//             }
//             case 9:
//             {
//                 lcd_clear(GRAY);
//                 break;
//             }
//             case 10:
//             {
//                 lcd_clear(LGRAY);
//                 break;
//             }
//             case 11:
//             {
//                 lcd_clear(BROWN);
//                 break;
//             }
//         }

//         lcd_show_string(10, 40, 240, 32, 32, "ESP32", RED);
//         lcd_show_string(10, 80, 240, 24, 24, "LCD TEST", RED);
//         lcd_show_string(10, 110, 240, 16, 16, "ATOM@ALIENTEK", RED);
//         x++;

//         if (x == 12)
//         {
//             x = 0;
//         }

//         LED_TOGGLE();
//         vTaskDelay(500);
//     }
// }
