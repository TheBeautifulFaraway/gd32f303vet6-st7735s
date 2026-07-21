#include "gd32f30x.h"
#include "lcd.h"
#include "lvgl.h"
#include "src/core/lv_obj_pos.h"
#include "src/core/lv_obj_style_gen.h"
#include "src/font/lv_font.h"
#include "src/misc/lv_color.h"
#include "src/misc/lv_timer.h"
#include "src/widgets/label/lv_label.h"
#include "stdio.h"
#include <stdint.h>

/* ---- USART0 解说口 ---- */
static void usart0_init(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);
}
static void u_putc(uint8_t b) {
    while (RESET == usart_flag_get(USART0, USART_FLAG_TBE))
        ;
    usart_data_transmit(USART0, (uint32_t)b);
}
static void u_puts(const char *s) {
    while (*s)
        u_putc((uint8_t)*s++);
    while (RESET == usart_flag_get(USART0, USART_FLAG_TC))
        ;
}

// lvgl显示缓冲区,大小是128*10像素,约2.5kB ram
#define BUF_LINES 10
static lv_color_t disp_buf[128 * BUF_LINES];

// lvgl刷新回调,v9版本
void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    // 1.设置窗口
    set_window(area->x1, area->y1, area->x2, area->y2);

    // 2.计算像素数量
    uint32_t len =
        (uint32_t)(area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);

    // 3.批量发送像素数据
    lcd_send_pixels((uint16_t *)px_map, len);

    // 4.通知lvgl刷新完成
    lv_display_flush_ready(disp);
}

// lvgl显示驱动注册
void lv_display_init() {
    // 创建128*160设备
    lv_display_t *disp = lv_display_create(128, 160);

    // 绑定刷新回调
    lv_display_set_flush_cb(disp, my_flush_cb);

    // 绑定缓冲区
    lv_display_set_buffers(disp, disp_buf, NULL, sizeof(disp_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

static void delay_ms(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 40000; j++)
            ;
}

// 全局计数器
static uint32_t tick_count = 4396;

// 全局变量label指针,方便在定时器里修改它的内容
static lv_obj_t *time_label;

// 定时器回调函数,每秒执行一次
static void sec_timer_cb(lv_timer_t *timer) {
    if (tick_count == 0) {
        tick_count = 4396;
    } else {
        tick_count--;
    }

    // 格式化时间
    char buf[16];
    uint8_t h = tick_count / 3600;
    uint8_t m = (tick_count % 3600) / 60;
    uint8_t s = tick_count % 60;
    sprintf(buf, "%02d:%02d:%02d", h, m, s);

    lv_label_set_text(time_label, buf);
}

int main(void) {
    // 1.系统时钟与systick配置
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);

    // 2.硬件初始化
    usart0_init();
    u_puts("\r\ngd32启动中...\r\n");
    lcd_init();
    u_puts("\r\nlcd初始化完成\r\n");

    // 3.lvgl初始化
    lv_init();
    lv_display_init();

    // 4.创建第一个UI,全屏红色背景
    lv_obj_t *src = lv_screen_active();
    lv_obj_set_style_bg_color(src, lv_color_black(), 0); // 红色
    lv_obj_set_style_bg_opa(src, LV_OPA_COVER, 0);

    // 5.设置文本和样式
    time_label = lv_label_create(src);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0); // 白色文字
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);

    // 6.设置居中
    lv_obj_center(time_label);

    // 7.创建定时器,1000ms调用一次
    lv_timer_create(sec_timer_cb, 1000, NULL);

    // 5.主循环
    while (1) {
        lv_timer_handler();
        delay_ms(5);
    }
}
