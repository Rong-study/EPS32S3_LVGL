#include "lv_test_ui.h"

// 定义UI组件变量
static lv_obj_t *win;             // 窗口对象
static lv_obj_t *ta_receive;      // 接收信息文本框
static lv_obj_t *ta_send;         // 发送信息文本框
static lv_obj_t *btn_send;        // 发送按钮
static lv_obj_t *kb;              // 键盘对象
static lv_obj_t *status_label;    // 状态标签，用于显示接收状态
// 增加任务句柄变量
static TaskHandle_t serial_task_handle = NULL;

// 串口接收缓冲区
#define MAX_RECV_LEN 1024
static char serial_buffer[MAX_RECV_LEN] = {0};
static uint16_t buffer_index = 0;  // 缓冲区当前索引，避免使用strncat带来的问题

// 检查串口是否初始化
static bool is_uart_initialized(void) {
    // 根据实际硬件情况实现此函数
    // 返回true表示串口已正确初始化
    return true;
}

// 发送按钮事件处理
static void send_btn_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        const char *text = lv_textarea_get_text(ta_send);
        if (text && *text) {
            // 检查串口是否初始化
            if (!is_uart_initialized()) {
                lv_label_set_text(status_label, "Error: UART not initialized");
                return;
            }

            // 调用串口发送函数
            uart_send_string(text);

            // 在接收框显示发送的消息
            char send_msg[128];
            snprintf(send_msg, sizeof(send_msg), "> Sent: %s\n", text);

            // 安全地添加到缓冲区
            uint16_t send_len = strlen(send_msg);
            if (buffer_index + send_len < MAX_RECV_LEN - 1) {
                memcpy(&serial_buffer[buffer_index], send_msg, send_len);
                buffer_index += send_len;
                serial_buffer[buffer_index] = '\0';
            } else {
                // 缓冲区已满，清空并提示
                buffer_index = 0;
                snprintf(serial_buffer, MAX_RECV_LEN, "Buffer full! Message truncated.\n");
                buffer_index = strlen(serial_buffer);
            }

            lv_textarea_set_text(ta_receive, serial_buffer);
            lv_textarea_set_cursor_pos(ta_receive, LV_TEXTAREA_CURSOR_LAST);

            // 清空发送框
            lv_textarea_set_text(ta_send, "");
            lv_label_set_text(status_label, "Message sent successfully");
        }
    }
}

// 窗口关闭事件处理 - 优化版本
static void win_close_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // 停止并删除串口数据任务
        if (serial_task_handle != NULL) {
            vTaskDelete(serial_task_handle);
            serial_task_handle = NULL;
        }

        // 清理串口缓冲区
        clear_serial_buffer();

        // 删除键盘（如果存在）
        if (kb) {
            lv_obj_del(kb);
            kb = NULL;
        }

        // 删除窗口（如果存在）
        if (win) {
            lv_obj_del(win);
            win = NULL;
        }

        // 重置所有UI组件指针为NULL，避免悬空引用
        ta_receive = NULL;
        ta_send = NULL;
        btn_send = NULL;
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

// 文本框焦点事件处理
static void ta_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        if (kb == NULL) {
            kb = lv_keyboard_create(lv_scr_act());
            lv_obj_add_event_cb(kb, kb_event_handler, LV_EVENT_ALL, NULL);
            lv_obj_set_size(kb, 280, 120);
        }

        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(kb, 10, 220);
    }
}

// 添加接收到的串口数据 - 修改为追加模式
void add_serial_data_task(void *pvParameters) {
    // 临时缓冲区用于每次读取
    char temp_buffer[UART_BUF_SIZE];
    int rx_len;
    
    while(1) {
        // 检查窗口是否还存在
        if (win == NULL) {
            vTaskDelete(NULL);
            return;
        }
        
        // 从串口获取数据（超时100ms）
        rx_len = uart_read_bytes(UART_PORT_NUM, temp_buffer, UART_BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        if (rx_len > 0) {
            temp_buffer[rx_len] = '\0';  // 添加字符串结束符
            
            // 安全地追加到主缓冲区
            uint16_t temp_len = strlen(temp_buffer);
            if (buffer_index + temp_len < MAX_RECV_LEN - 1) {
                memcpy(&serial_buffer[buffer_index], temp_buffer, temp_len);
                buffer_index += temp_len;
                serial_buffer[buffer_index] = '\0';
            } else {
                // 缓冲区已满，清空并提示
                buffer_index = 0;
                snprintf(serial_buffer, MAX_RECV_LEN, "Buffer full! Message truncated.\n");
                buffer_index = strlen(serial_buffer);
            }

            // 更新文本框显示（保留历史数据）
            if (ta_receive) {
                lv_textarea_set_text(ta_receive, serial_buffer);
                lv_textarea_set_cursor_pos(ta_receive, LV_TEXTAREA_CURSOR_LAST);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // 短暂延时
    }
}


// 清空接收缓冲区 - 修改为仅清空缓冲区
void clear_serial_buffer() {
    // 重置缓冲区（保留最后一条提示信息）
    buffer_index = 0;
    snprintf(serial_buffer, MAX_RECV_LEN, "Buffer cleared\nWaiting for data...");
    buffer_index = strlen(serial_buffer);
    
    // 更新文本框显示
    if (ta_receive) {
        lv_textarea_set_text(ta_receive, serial_buffer);
    }
    if (status_label) {
        lv_label_set_text(status_label, "Status: Buffer cleared");
    }
}

// 创建串口监控窗口
void create_serial_monitor_window(void) {
    // 如果窗口已存在，先关闭
    if (win) {
        lv_obj_del(win);
        win = NULL;
    }

    // 创建窗口，增加高度以容纳状态标签
    win = lv_win_create(lv_scr_act(), 30);
    lv_win_add_title(win, "UART Monitor");

    // 添加标题栏关闭按钮并设置事件
    lv_obj_t *close_btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE, 40);
    lv_obj_add_event_cb(close_btn, win_close_event_handler, LV_EVENT_CLICKED, NULL);

    // 调整窗口大小以容纳新增的状态标签
    lv_obj_set_size(win, 300, 240);
    lv_obj_center(win);

    // 创建接收文本框
    ta_receive = lv_textarea_create(lv_win_get_content(win));
    lv_obj_set_size(ta_receive, 280, 80);
    lv_obj_set_pos(ta_receive, 10, 10);
    lv_textarea_set_text(ta_receive, "Waiting For Data...");
    lv_textarea_set_cursor_click_pos(ta_receive, true);
    lv_textarea_set_placeholder_text(ta_receive, "no data");
    lv_obj_set_scrollbar_mode(ta_receive, LV_SCROLLBAR_MODE_AUTO);
    lv_textarea_set_one_line(ta_receive, false);
    lv_obj_add_flag(ta_receive, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 创建发送文本框
    ta_send = lv_textarea_create(lv_win_get_content(win));
    lv_obj_set_size(ta_send, 280, 40);
    lv_obj_set_pos(ta_send, 10, 100);
    lv_textarea_set_placeholder_text(ta_send, "Enter message...");
    lv_textarea_set_one_line(ta_send, false);
    lv_obj_add_event_cb(ta_send, ta_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建发送按钮
    btn_send = lv_btn_create(lv_win_get_content(win));
    lv_obj_set_size(btn_send, 135, 30);
    lv_obj_set_pos(btn_send, 10, 150);
    lv_obj_t *label_send = lv_label_create(btn_send);
    lv_label_set_text(label_send, "Send Message");
    lv_obj_add_event_cb(btn_send, send_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_send, lv_color_white(), LV_STATE_DEFAULT);

    // 创建清空按钮
    lv_obj_t *btn_clear = lv_btn_create(lv_win_get_content(win));
    lv_obj_set_size(btn_clear, 135, 30);
    lv_obj_set_pos(btn_clear, 150, 150);
    lv_obj_t *label_clear = lv_label_create(btn_clear);
    lv_label_set_text(label_clear, "Clear Buffer");
    lv_obj_add_event_cb(btn_clear, (lv_event_cb_t)clear_serial_buffer, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0x2196F3), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_clear, lv_color_white(), LV_STATE_DEFAULT);

    // 添加状态标签，用于显示接收状态和错误信息
    status_label = lv_label_create(lv_win_get_content(win));
    lv_obj_set_size(status_label, 135, 30);
    lv_obj_set_pos(status_label, 155, 190);
    lv_label_set_text(status_label, "Status: Ready");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
}

// 主启动程序
void lv_test_ui(void) {
    // 确保串口已初始化
    static bool uart_initialized = false;
    if (!uart_initialized) {
        uart_comm_init();  // 初始化UART通信
        uart_initialized = true;
    }
    
    // 清理串口缓冲区
    clear_serial_buffer();
    
    // 创建串口监控窗口
    create_serial_monitor_window();
    
    // 如果任务不存在，创建任务
    if (serial_task_handle == NULL) {
        xTaskCreate(add_serial_data_task, 
                    "add_serial_data_task", 
                    4096, 
                    NULL, 
                    5, 
                    &serial_task_handle);
    }
}