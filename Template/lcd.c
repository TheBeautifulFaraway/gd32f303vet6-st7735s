#include "lcd.h"
#include "gd32f30x_gpio.h"
#include <stdint.h>

// 延迟函数
static void delay_ms(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 40000; j++)
            ;
}

/* ---- SPI0：模式0 / MSB / 8bit / 软件NSS / 慢速 ---- */
void spi_send_byte(uint8_t b) {
    while (RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE))
        ;
    spi_i2s_data_transmit(SPI0, (uint16_t)b);
    while (SET == spi_i2s_flag_get(SPI0, SPI_FLAG_TRANS))
        ; /* GD32 真名，非 BSY */
}

void lcd_send_pixels(uint16_t *color_p, uint32_t len) {
    // 拉低cs激活片选,拉高dc切换到数据模式
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_DC);

    // 遍历color内容发送到显示器
    while (len--) {
        uint16_t c = *color_p++;

        spi_send_byte((uint8_t)(c >> 8));
        spi_send_byte((uint8_t)(c & 0xff));
    }

    // 拉高片选,结束传输
    gpio_bit_set(GPIOC, PIN_CS);
}

// spi0初始化
void spi0_init(void) {
    spi_parameter_struct sp;
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_SPI0);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              GPIO_PIN_5 | GPIO_PIN_7);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PIN_CS | PIN_DC | PIN_RES);
    gpio_bit_set(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_RES);

    spi_struct_para_init(&sp); /* GD32 真名 */
    sp.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    sp.device_mode = SPI_MASTER;
    sp.frame_size = SPI_FRAMESIZE_8BIT;
    sp.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE; /* 模式0 */
    sp.nss = SPI_NSS_SOFT;
    sp.prescale = SPI_PSC_8; /* 飞线求稳；出图后改 PSC_8 */
    sp.endian = SPI_ENDIAN_MSB;
    spi_init(SPI0, &sp); /* 注意：无 spi_deinit */
    spi_enable(SPI0);
}

// st7735命令封装
// 发送一个命令
void lcd_cmd(uint8_t c) {
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_reset(GPIOC, PIN_DC);
    spi_send_byte(c);
    gpio_bit_set(GPIOC, PIN_CS);
}

// 发送一个数据
void lcd_data1(uint8_t d) {
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_DC);
    spi_send_byte(d);
    gpio_bit_set(GPIOC, PIN_CS);
}

// 连续发送数据
void lcd_dataN(const uint8_t *p, uint16_t n) {
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_DC);
    while (n--)
        spi_send_byte(*p++);
    gpio_bit_set(GPIOC, PIN_CS);
}

// 复位
void lcd_reset(void) {
    gpio_bit_set(GPIOC, PIN_RES);
    delay_ms(10);
    gpio_bit_reset(GPIOC, PIN_RES);
    delay_ms(20);
    gpio_bit_set(GPIOC, PIN_RES);
    delay_ms(120);
}

// st7735s初始化序列
void lcd_panel_init(void) {
    uint8_t d[6];
    lcd_reset();
    lcd_cmd(0x01);
    delay_ms(150);
    lcd_cmd(0x11);
    delay_ms(500);
    d[0] = 0x01;
    d[1] = 0x2C;
    d[2] = 0x2D;
    lcd_cmd(0xB1);
    lcd_dataN(d, 3);
    d[0] = 0x01;
    d[1] = 0x2C;
    d[2] = 0x2D;
    lcd_cmd(0xB2);
    lcd_dataN(d, 3);
    d[0] = 0x01;
    d[1] = 0x2C;
    d[2] = 0x2D;
    d[3] = 0x01;
    d[4] = 0x2C;
    d[5] = 0x2D;
    lcd_cmd(0xB3);
    lcd_dataN(d, 6);
    lcd_cmd(0xB4);
    lcd_data1(0x07);
    d[0] = 0xA2;
    d[1] = 0x02;
    d[2] = 0x84;
    lcd_cmd(0xC0);
    lcd_dataN(d, 3);
    lcd_cmd(0xC1);
    lcd_data1(0xC5);
    d[0] = 0x0A;
    d[1] = 0x00;
    lcd_cmd(0xC2);
    lcd_dataN(d, 2);
    d[0] = 0x8A;
    d[1] = 0x2A;
    lcd_cmd(0xC3);
    lcd_dataN(d, 2);
    d[0] = 0x8A;
    d[1] = 0xEE;
    lcd_cmd(0xC4);
    lcd_dataN(d, 2);
    lcd_cmd(0xC5);
    lcd_data1(0x0E);
    lcd_cmd(0x20);
    lcd_cmd(0x3A);
    lcd_data1(0x05);
    lcd_cmd(0x36);
    lcd_data1(0x00);
    lcd_cmd(0x29);
    delay_ms(100);
}

// 窗口设置
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t d[4];
    lcd_cmd(0x2A);
    d[0] = x0 >> 8;
    d[1] = x0 & 0xFF;
    d[2] = x1 >> 8;
    d[3] = x1 & 0xFF;
    lcd_dataN(d, 4);
    lcd_cmd(0x2B);
    d[0] = y0 >> 8;
    d[1] = y0 & 0xFF;
    d[2] = y1 >> 8;
    d[3] = y1 & 0xFF;
    lcd_dataN(d, 4);
    lcd_cmd(0x2C);
}

// 初始化总入口
void lcd_init() {
    spi0_init();
    lcd_panel_init();
}