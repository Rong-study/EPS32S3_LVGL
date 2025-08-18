#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <stdint.h>
#include "lcd.h"

/**
 * @brief       初始化LCD显示
 * @param       无
 * @retval      无
 */
void lcd_display_init(void);

/**
 * @brief       创建LCD显示任务
 * @param       无
 * @retval      无
 */
void lcd_create_display_task(void);

/**
 * @brief       更新LCD显示内容
 * @param       data: 要显示的数据
 * @retval      无
 */
void lcd_update_display(const char* data);

#endif /* LCD_DISPLAY_H */
    