#ifndef __LV_TEST_UI_H
#define __LV_TEST_UI_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "lcd.h"
#include "string.h"
#include "usart.h"
#include "lcd_display.h"
#include "myiic.h"
#include "xl9555.h"

void lv_test_ui(void);
#endif /* __LV_TEST_UI_H */