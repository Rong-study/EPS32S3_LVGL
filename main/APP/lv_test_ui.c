#include "lv_test_ui.h"


void lv_test_ui(void)
{
    //初始化显示内容
    lcd_clear(WHITE);
    lcd_show_string(10, 10, 240, 32, 32, "UART STM32-ESP32", RED);
    lcd_show_string(10, 60, 240, 20, 16, "-------------------", BLUE);
    lcd_show_string(10, 80, 240, 16, 24, "RX:", BLACK);
// 创建通信和显示任务
    uart_create_receive_task();
    uart_create_send_task();
    lcd_create_display_task();
}
