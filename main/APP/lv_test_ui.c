#include "lv_test_ui.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

// 定义UI组件变量
static lv_obj_t *win;             // 窗口对象
static lv_obj_t *ta_receive;      // 接收信息文本框
static lv_obj_t *ta_send;         // 发送信息文本框
static lv_obj_t *btn_send;        // 发送按钮
static lv_obj_t *btn_clear;       // 清除按钮
static lv_obj_t *kb;              // 键盘对象
static lv_obj_t *status_label;    // 状态标签

// 任务和流缓冲区
static TaskHandle_t serial_task_handle = NULL;
static StreamBufferHandle_t uart_stream_buf = NULL;
static lv_timer_t *update_timer = NULL;  // UI更新定时器

// 流缓冲区配置
#define STREAM_BUF_SIZE 2048  // 流缓冲区大小
#define TRIGGER_LEVEL 64     // 触发读取的字节数

// 检查串口是否初始化
static bool is_uart_initialized(void) {
    return true; // 实际项目中需要实现
}

// 发送按钮事件处理
static void send_btn_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        const char *text = lv_textarea_get_text(ta_send);
        if (text && *text) {
            if (!is_uart_initialized()) {
                if (status_label && lv_obj_is_valid(status_label)) {
                    lv_label_set_text(status_label, "Error: UART not initialized");
                }
                return;
            }

            // 发送数据
            uart_send_string(text);

            // 在接收框显示发送的消息
            char send_msg[128];
            snprintf(send_msg, sizeof(send_msg), "> Sent: %s\n", text);
            // 直接添加到接收框
            if (ta_receive && lv_obj_is_valid(ta_receive)) {
                lv_textarea_add_text(ta_receive, send_msg);
                lv_textarea_set_cursor_pos(ta_receive, LV_TEXTAREA_CURSOR_LAST);
            }

            // 清空发送框
            lv_textarea_set_text(ta_send, "");
            if (status_label && lv_obj_is_valid(status_label)) {
                lv_label_set_text(status_label, "Message sent");
            }
        }
    }
}

// 窗口关闭事件处理
static void win_close_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // 停止串口任务
        if (serial_task_handle != NULL) {
            vTaskDelete(serial_task_handle);
            serial_task_handle = NULL;
        }
        // 删除定时器
        if (update_timer) {
            lv_timer_del(update_timer);
            update_timer = NULL;
        }
        // 删除流缓冲区
        if (uart_stream_buf != NULL) {
            vStreamBufferDelete(uart_stream_buf);
            uart_stream_buf = NULL;
        }
        // 删除UI对象
        if (kb && lv_obj_is_valid(kb)) {
            lv_obj_del(kb);
            kb = NULL;
        }
        if (win && lv_obj_is_valid(win)) {
            lv_obj_del(win);
            win = NULL;
        }
        // 重置指针
        ta_receive = NULL;
        ta_send = NULL;
        btn_send = NULL;
        btn_clear = NULL;
        status_label = NULL;
        // 回到主页面
        lv_mian_ui();
    }
}

// 键盘事件处理
static void kb_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);
    
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// 全局点击事件处理 - 点击屏幕其他地方关闭键盘
static void global_click_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    // 如果点击的不是键盘或发送文本框，则关闭键盘
    if (code == LV_EVENT_CLICKED && kb && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
        if (target != kb && target != ta_send) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 文本框焦点事件处理 - 优化版本
static void ta_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        if (kb) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        if (kb) {
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 串口数据接收任务
void uart_receive_task(void *pvParameters) {
    uint8_t temp_buffer[UART_BUF_SIZE];
    while(1) {
        // 从串口读取数据
        int rx_len = uart_read_bytes(UART_PORT_NUM, temp_buffer, UART_BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        if (rx_len > 0) {
            // 写入流缓冲区
            if (uart_stream_buf != NULL) {
                size_t bytes_written = xStreamBufferSend(uart_stream_buf, temp_buffer, rx_len, 0);
                // 如果缓冲区满，触发定时器立即处理
                if (bytes_written < (size_t)rx_len) {
                    if (update_timer) {
                        lv_timer_reset(update_timer);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));  // 短暂延时
    }
}

// 清空接收缓冲区
void clear_serial_buffer() {
    if (ta_receive && lv_obj_is_valid(ta_receive)) {
        lv_textarea_set_text(ta_receive, "Buffer cleared\nWaiting for data...");
    }
    if (status_label && lv_obj_is_valid(status_label)) {
        lv_label_set_text(status_label, "Status: Buffer cleared");
    }
    // 清空流缓冲区
    if (uart_stream_buf != NULL) {
        xStreamBufferReset(uart_stream_buf);
    }
}

// LVGL定时器回调 - 安全更新UI
static void update_ui_timer(lv_timer_t *timer) {
    if (uart_stream_buf == NULL) return;
    // 检查缓冲区中可读数据量
    size_t available = xStreamBufferBytesAvailable(uart_stream_buf);
    if (available == 0) return;
    // 创建缓冲区读取数据
    uint8_t buffer[256];
    size_t bytes_read = xStreamBufferReceive(uart_stream_buf, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        // 确保字符串以null结尾
        buffer[bytes_read] = '\0';
        // 安全更新UI
        if (ta_receive && lv_obj_is_valid(ta_receive)) {
            // 直接添加文本到文本框
            lv_textarea_add_text(ta_receive, (char*)buffer);
            // 只有在没有用户交互时才自动滚动
            if (!lv_obj_is_scrolling(ta_receive)) {
                lv_textarea_set_cursor_pos(ta_receive, LV_TEXTAREA_CURSOR_LAST);
            }
        }
    }
}

// 创建串口监控窗口
void create_serial_monitor_window(void) {
    // 清除屏幕的滚动标志
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    
    // 如果窗口已存在，先关闭
    if (win && lv_obj_is_valid(win)) {
        lv_obj_del(win);
        win = NULL;
    }
    
    // 创建窗口
    win = lv_win_create(lv_scr_act(), 20);
    lv_win_add_title(win, "UART Monitor");
    lv_obj_set_size(win, 320, 240);
    lv_obj_center(win);
    
    // 禁用窗口的滚动功能
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lv_win_get_content(win), LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建接收文本框
    ta_receive = lv_textarea_create(lv_win_get_content(win));
    lv_obj_set_size(ta_receive, 300, 110);
    lv_obj_set_pos(ta_receive, 0, 0);
    lv_textarea_set_text(ta_receive, "Waiting For Data...");
    lv_textarea_set_cursor_click_pos(ta_receive, false);
    lv_textarea_set_placeholder_text(ta_receive, "no data");
    lv_obj_set_scrollbar_mode(ta_receive, LV_SCROLLBAR_MODE_AUTO);
    lv_textarea_set_one_line(ta_receive, false);
    lv_obj_add_flag(ta_receive, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 创建发送文本框
    ta_send = lv_textarea_create(lv_win_get_content(win));
    lv_obj_set_size(ta_send, 200, 40);
    lv_obj_set_pos(ta_send, 0, 120);
    lv_textarea_set_placeholder_text(ta_send, "Enter message...");
    lv_textarea_set_one_line(ta_send, false);
    lv_obj_add_event_cb(ta_send, ta_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建发送按钮
    btn_send = lv_btn_create(lv_win_get_content(win));
    lv_obj_set_size(btn_send, 60, 40);
    lv_obj_set_pos(btn_send, 210, 120);
    lv_obj_t *label_send = lv_label_create(btn_send);
    lv_label_set_text(label_send, "Send");
    lv_obj_add_event_cb(btn_send, send_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_send, lv_color_white(), LV_STATE_DEFAULT);

    // 创建清除按钮
    btn_clear = lv_btn_create(lv_win_get_content(win));
    lv_obj_set_size(btn_clear, 60, 40);
    lv_obj_set_pos(btn_clear, 270, 120);
    lv_obj_t *label_clear = lv_label_create(btn_clear);
    lv_label_set_text(label_clear, "Clear");
    lv_obj_add_event_cb(btn_clear, (lv_event_cb_t)clear_serial_buffer, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0x2196F3), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_clear, lv_color_white(), LV_STATE_DEFAULT);


    // 在窗口内创建键盘
    kb = lv_keyboard_create(lv_win_get_content(win));
    lv_obj_set_size(kb, 300, 100);
    lv_obj_set_pos(kb, 10, 135);
     // 放在屏幕底部
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); // 初始隐藏
    lv_obj_add_event_cb(kb, kb_event_handler, LV_EVENT_ALL, NULL);
    // 设置键盘的文本区域
    lv_keyboard_set_textarea(kb, NULL);
    // 添加全局点击事件 - 点击其他地方关闭键盘
    lv_obj_add_event_cb(lv_scr_act(), global_click_event_handler, LV_EVENT_CLICKED, NULL);
}

// 主启动程序
void lv_test_ui(void) {
    // 确保串口已初始化
    static bool uart_initialized = false;
    if (!uart_initialized) {
        //uart_comm_init();
        uart_initialized = true;
    }
    // 创建流缓冲区（如果尚未创建）
    if (uart_stream_buf == NULL) {
        uart_stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, TRIGGER_LEVEL);
    }
    // 创建串口监控窗口
    create_serial_monitor_window();
    // 创建LVGL定时器用于安全更新UI
    if (update_timer == NULL) {
        update_timer = lv_timer_create(update_ui_timer, 30, NULL); // 每30ms检查一次
        lv_timer_set_repeat_count(update_timer, -1); // 无限重复
    }
    // 创建串口接收任务（如果尚未创建）
    if (serial_task_handle == NULL) {
        xTaskCreate(uart_receive_task,
                    "uart_receive_task",
                    4096,
                    NULL,
                    6,  // 提高优先级，确保数据接收不被阻塞
                    &serial_task_handle);
    }
}