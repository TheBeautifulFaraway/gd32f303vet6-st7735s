#include "gd32f30x.h"
#include "lcd.h"
#include "lvgl.h"
#include "stdio.h"
#include <stdint.h>
#include "digit_0_bottom.h"
#include "digit_0_top.h"

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
    while (RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    usart_data_transmit(USART0, (uint32_t)b);
}

static void u_puts(const char *s) {
    while (*s) u_putc((uint8_t)*s++);
    while (RESET == usart_flag_get(USART0, USART_FLAG_TC));
}

// lvgl显示缓冲区,大小是160*10像素,约3.2kB ram (横屏宽度改为160)
#define BUF_LINES 10
static lv_color_t disp_buf[160 * BUF_LINES];

// lvgl刷新回调,v9版本
void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    set_window(area->x1, area->y1, area->x2, area->y2);
    uint32_t len = (uint32_t)(area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    lcd_send_pixels((uint16_t *)px_map, len);
    lv_display_flush_ready(disp);
}

// lvgl显示驱动注册
void lv_display_init() {
    // 创建160*128设备(横屏)
    lv_display_t *disp = lv_display_create(160, 128);
    lv_display_set_flush_cb(disp, my_flush_cb);
    lv_display_set_buffers(disp, disp_buf, NULL, sizeof(disp_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
}

static void delay_ms(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 40000; j++);
}

int main(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);
    
    usart0_init();
    u_puts("\r\ngd32启动中...\r\n");
    lcd_init();
    u_puts("\r\nlcd初始化完成(横屏模式)\r\n");
    
    lv_init();
    lv_display_init();
    
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    // ====== 核心修改：左中右三个数字布局 ======
    // 屏幕 160x128，数字 50x120
    // X坐标：0, 55, 110 (第一个贴左，间隔5列)
    // Y坐标：0 (上), 64 (下，中间空4行)
    int16_t x_pos[3] = {0, 55, 110};
    int16_t y_top = 0;
    int16_t y_bottom = 64; 
    
    for (int i = 0; i < 3; i++) {
        // 上半瓣
        lv_obj_t *img_top = lv_image_create(scr);
        lv_image_set_src(img_top, &digit_0_top);
        lv_obj_set_pos(img_top, x_pos[i], y_top);
        
        // 下半瓣
        lv_obj_t *img_bottom = lv_image_create(scr);
        lv_image_set_src(img_bottom, &digit_0_bottom);
        lv_obj_set_pos(img_bottom, x_pos[i], y_bottom);
    }
    // ==========================================
    
    u_puts("\r\n三个数字显示完成\r\n");
    
    while (1) {
        lv_timer_handler();
        delay_ms(5);
    }
}