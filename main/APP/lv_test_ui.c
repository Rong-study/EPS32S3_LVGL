#include "lv_test_ui.h"


// 定义UI组件变量
static lv_obj_t *win;             // 窗口对象
static lv_obj_t *ta_receive;      // 接收信息文本框
static lv_obj_t *ta_send;         // 发送信息文本框
static lv_obj_t *btn_send;        // 发送按钮
static lv_obj_t *btn_clear;       // 清除按钮
static lv_obj_t *kb;              // 键盘对象

// 任务和流缓冲区
static TaskHandle_t serial_task_handle = NULL;
static StreamBufferHandle_t uart_stream_buf = NULL;
static lv_timer_t *update_timer = NULL;  // UI更新定时器

// 流缓冲区配置
#define STREAM_BUF_SIZE 4096     // 流缓冲区大小
#define TRIGGER_LEVEL 128        // 触发级别
#define UART_BUF_SIZE 512        // UART缓冲区大小
#define MAX_DISPLAY_LINES 10    // 最大显示行数
#define MAX_UPDATE_CHUNK 256     // 每次更新最大字节数

// 优化结构：记录文本框关键状态
typedef struct {
    uint32_t line_count;           // 当前行数
    char last_char;                // 最后一个字符（用于行数计算）
} text_metrics_t;

static text_metrics_t rx_metrics = {0};

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
                return;
            }

            // 发送数据
            uart_send_string(text);

            // 在接收框显示发送的消息
            char send_msg[128];
            snprintf(send_msg, sizeof(send_msg), "> Sent: %s\n", text);
            
            // 直接添加到接收框
            if (ta_receive && lv_obj_is_valid(ta_receive)) {
                // 更新行数计数
                rx_metrics.line_count++;
                
                lv_textarea_add_text(ta_receive, send_msg);
                lv_textarea_set_cursor_pos(ta_receive, LV_TEXTAREA_CURSOR_LAST);
            }

            // 清空发送框
            lv_textarea_set_text(ta_send, "");
        }
    }
}

// 键盘事件处理
static void kb_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);
    
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        if (ta_send) {
            lv_obj_clear_state(ta_send, LV_STATE_FOCUSED);
        }
    }
}

// 全局点击事件处理 - 点击屏幕其他地方关闭键盘
static void global_click_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    
    // 如果点击的不是键盘或发送文本框，则关闭键盘
    if (code == LV_EVENT_CLICKED && kb && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
        if (target != kb && target != ta_send && target != btn_send && target != btn_clear) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            if (ta_send) {
                lv_obj_clear_state(ta_send, LV_STATE_FOCUSED);
            }
        }
    }
}

// 文本框焦点事件处理
static void ta_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        if (kb) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_move_foreground(kb);
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
        int rx_len = uart_read_bytes(UART_PORT_NUM, temp_buffer, UART_BUF_SIZE - 1, 
                                    pdMS_TO_TICKS(10)); // 减少阻塞时间
        
        if (rx_len > 0) {
            // 写入流缓冲区
            if (uart_stream_buf != NULL) {
                // 使用线程安全的发送方式
                size_t bytes_written = xStreamBufferSend(uart_stream_buf, temp_buffer, rx_len, portMAX_DELAY);
                
                // 如果缓冲区满，触发定时器立即处理
                if (bytes_written < (size_t)rx_len && update_timer) {
                    lv_timer_reset(update_timer);
                }
            }
        }
        taskYIELD(); // 短暂让出CPU
    }
}

// 清空接收缓冲区
static void clear_serial_buffer(lv_event_t *e) {
    if (ta_receive && lv_obj_is_valid(ta_receive)) {
        lv_textarea_set_text(ta_receive, "Buffer cleared\nWaiting for data...");
        // 重置行数计数
        rx_metrics.line_count = 0;
        rx_metrics.last_char = '\0';
    }
    // 清空流缓冲区
    if (uart_stream_buf != NULL) {
        xStreamBufferReset(uart_stream_buf);
    }
}

// 计算新文本中的行数
static uint32_t count_newlines(const char *buffer, size_t len, char *last_char) {
    uint32_t count = 0;
    
    for (size_t i = 0; i < len; i++) {
        // 只有当last_char不是\r且当前字符是\n时计数
        if (*last_char != '\r' && buffer[i] == '\n') {
            count++;
        }
        *last_char = buffer[i];
    }
    
    return count;
}

// LVGL定时器回调 - 安全更新UI（优化版本）
static void update_ui_timer(lv_timer_t *timer) {
    if (uart_stream_buf == NULL) return;
    
    // 检查缓冲区中可读数据量
    size_t available = xStreamBufferBytesAvailable(uart_stream_buf);
    if (available == 0) return;
    
    // 限制每次处理的最大数据量
    size_t bytes_to_read = (available > MAX_UPDATE_CHUNK) ? MAX_UPDATE_CHUNK : available;
    
    // 创建缓冲区读取数据
    uint8_t buffer[MAX_UPDATE_CHUNK + 1]; // +1 for null terminator
    size_t bytes_read = xStreamBufferReceive(uart_stream_buf, buffer, bytes_to_read, 0);
    
    if (bytes_read > 0) {
        // 确保字符串以null结尾
        buffer[bytes_read] = '\0';
        
        // 计算新增行数
        uint32_t new_lines = count_newlines((char*)buffer, bytes_read, &rx_metrics.last_char);
        rx_metrics.line_count += new_lines;
        
        // 安全更新UI
        if (ta_receive && lv_obj_is_valid(ta_receive)) {
            // 检查当前内容行数，如果超过最大值则清除
            if (rx_metrics.line_count > MAX_DISPLAY_LINES) {
                lv_textarea_set_text(ta_receive, "");
                rx_metrics.line_count = new_lines; // 重置计数器
            }
            
            // 直接添加文本到文本框
            lv_textarea_add_text(ta_receive, (char*)buffer);
            
            // 只有在没有用户交互时才自动滚动
            if (!lv_obj_is_scrolling(ta_receive)) {
                lv_textarea_set_cursor_pos(ta_receive, LV_TEXTAREA_CURSOR_LAST);
            }
        }
    }
}

// 优化版本 - 创建键盘对象
static lv_obj_t* create_keyboard(lv_obj_t *parent) {
    lv_obj_t *kb = lv_keyboard_create(parent);
    
    // 优化键盘样式
    lv_obj_set_size(kb, lv_pct(95), 120);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 设置键盘为透明背景
    lv_obj_set_style_bg_opa(kb, LV_OPA_90, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    
    // 设置按键样式
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x444444), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(kb, lv_color_white(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    
    return kb;
}

// 销毁UI资源
static void destroy_ui_resources() {
    // 删除定时器
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    // 删除任务
    if (serial_task_handle) {
        vTaskDelete(serial_task_handle);
        serial_task_handle = NULL;
    }
    
    // 删除流缓冲区
    if (uart_stream_buf) {
        vStreamBufferDelete(uart_stream_buf);
        uart_stream_buf = NULL;
    }
}

// 创建串口监控窗口
void create_serial_monitor_window(void) {
    // 清除屏幕的滚动标志
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    
    // 如果窗口已存在，先关闭
    if (win && lv_obj_is_valid(win)) {
        destroy_ui_resources();
        lv_obj_del(win);
        win = NULL;
    }
    
    // 创建窗口
    win = lv_win_create(lv_scr_act(), 20);
    lv_win_add_title(win, "UART Monitor");
    lv_obj_set_size(win, lv_pct(100), lv_pct(100));
    lv_obj_center(win);
    
    // 优化窗口样式
    lv_obj_set_style_bg_color(win, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(win, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(win, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(win, lv_color_hex(0x444444), LV_PART_MAIN);
   // 禁用窗口的滚动功能 - 使用更可靠的方法
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLL_CHAIN);

    // 禁用窗口的滚动功能
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lv_win_get_content(win), LV_OBJ_FLAG_SCROLLABLE);
    
    // 设置窗口内容样式
    lv_obj_set_style_pad_all(lv_win_get_content(win), 5, 0);

    // 创建主容器 - 使用Flex布局
    lv_obj_t *main_container = lv_obj_create(lv_win_get_content(win));
    lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(main_container, 0, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_TRANSP, 0);

    // 创建接收文本框容器
    lv_obj_t *receive_container = lv_obj_create(main_container);
    lv_obj_set_size(receive_container, lv_pct(100), lv_pct(70));
    lv_obj_set_style_pad_all(receive_container, 0, 0);
    lv_obj_set_style_border_width(receive_container, 0, 0);
    lv_obj_set_style_bg_opa(receive_container, LV_OPA_TRANSP, 0);

    // 创建接收文本框
    ta_receive = lv_textarea_create(receive_container);
    lv_obj_set_size(ta_receive, lv_pct(100), lv_pct(100));
    lv_textarea_set_text(ta_receive, "Waiting For Data...");
    
    // 优化接收文本框样式
    lv_obj_set_style_bg_color(ta_receive, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_color(ta_receive, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_receive, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta_receive, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta_receive, 5, 0);
    
    lv_textarea_set_cursor_click_pos(ta_receive, false);
    lv_textarea_set_placeholder_text(ta_receive, "No data received");
    lv_obj_set_scrollbar_mode(ta_receive, LV_SCROLLBAR_MODE_AUTO);
    lv_textarea_set_one_line(ta_receive, false);
    lv_obj_add_flag(ta_receive, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 创建底部容器 - 用于发送区域
    lv_obj_t *bottom_container = lv_obj_create(main_container);
    lv_obj_set_size(bottom_container, lv_pct(100), lv_pct(30));
    lv_obj_set_flex_flow(bottom_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bottom_container, 0, 0);
    lv_obj_set_style_border_width(bottom_container, 0, 0);
    lv_obj_set_style_bg_opa(bottom_container, LV_OPA_TRANSP, 0);

    // 创建发送文本框容器
    lv_obj_t *send_container = lv_obj_create(bottom_container);
    lv_obj_set_size(send_container, lv_pct(65), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(send_container, 1);
    lv_obj_set_style_pad_all(send_container, 0, 0);
    lv_obj_set_style_border_width(send_container, 0, 0);
    lv_obj_set_style_bg_opa(send_container, LV_OPA_TRANSP, 0);

    // 创建发送文本框
    ta_send = lv_textarea_create(send_container);
    lv_obj_set_size(ta_send, lv_pct(100), LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ta_send, "Enter message...");
    lv_textarea_set_one_line(ta_send, true); // 单行模式
    lv_obj_add_event_cb(ta_send, ta_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_send, ta_event_handler, LV_EVENT_DEFOCUSED, NULL);
    
    // 优化发送文本框样式
    lv_obj_set_style_bg_color(ta_send, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_send, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta_send, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(ta_send, 5, LV_PART_MAIN);
    
    // 创建按钮容器
    lv_obj_t *btn_container = lv_obj_create(bottom_container);
    lv_obj_set_size(btn_container, lv_pct(35), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);

    // 创建发送按钮
    btn_send = lv_btn_create(btn_container);
    lv_obj_set_size(btn_send, lv_pct(50), 40);
    lv_obj_t *label_send = lv_label_create(btn_send);
    lv_label_set_text(label_send, "Send");
    lv_obj_add_event_cb(btn_send, send_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 优化按钮样式
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_send, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_send, 5, LV_STATE_DEFAULT);

    // 创建清除按钮
    btn_clear = lv_btn_create(btn_container);
    lv_obj_set_size(btn_clear, lv_pct(50), 40);
    lv_obj_t *label_clear = lv_label_create(btn_clear);
    lv_label_set_text(label_clear, "Clear");
    lv_obj_add_event_cb(btn_clear, clear_serial_buffer, LV_EVENT_CLICKED, NULL);
    
    // 优化按钮样式
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0x2196F3), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_clear, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_clear, 5, LV_STATE_DEFAULT);

    // 在窗口内创建键盘
    kb = create_keyboard(lv_win_get_content(win));
    lv_obj_add_event_cb(kb, kb_event_handler, LV_EVENT_ALL, NULL);
    
    // 添加全局点击事件 - 点击其他地方关闭键盘
    lv_obj_add_event_cb(lv_scr_act(), global_click_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 重置行数计数
    rx_metrics.line_count = 0;
    rx_metrics.last_char = '\0';
}

// 主启动程序
void lv_test_ui(void) {
    
    // 创建流缓冲区（如果尚未创建）
    if (uart_stream_buf == NULL) {
        uart_stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, TRIGGER_LEVEL);
    }
    
    // 创建串口监控窗口
    create_serial_monitor_window();
    
    // 创建LVGL定时器用于安全更新UI
    if (update_timer == NULL) {
        // 提高定时器频率为20ms以提高响应性
        update_timer = lv_timer_create(update_ui_timer, 20, NULL);
        lv_timer_set_repeat_count(update_timer, -1); // 无限重复
    }
    
    // 创建串口接收任务（如果尚未创建）
    if (serial_task_handle == NULL) {
        xTaskCreatePinnedToCore(uart_receive_task,
                    "uart_receive_task",
                    4096,
                    NULL,
                    6,  // 提高优先级，确保数据接收不被阻塞
                    &serial_task_handle,
                    1); // 指定在核心1上运行
    }
}