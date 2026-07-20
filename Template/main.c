#include "gd32f30x.h"

/* ===================================================================
 * ST7735S 出图验证 v2：开场填黑(白->黑强对比) + 每秒轮换 R/G/B
 * 接线：SCL=PA5 SDA=PA7 CS=PC0 DC=PC1 RES=PC2  BLK悬空  VCC=3V3
 * 全程阻塞 SPI、裸机、无 DMA/RTOS。SPI 默认慢速(PSC_64)求飞线稳。
 * 旋钮：还白 -> 把 SPI_PSC_64 改 SPI_PSC_256；出图后改 SPI_PSC_8 提速
 * =================================================================== */
#define LCD_W 128
#define LCD_H 160
#define C_RED 0xF800
#define C_GREEN 0x07E0
#define C_BLUE 0x001F
#define C_BLACK 0x0000

#define PIN_CS   GPIO_PIN_2   /* 模块 CS  (第7脚) -> PC2 */
#define PIN_DC   GPIO_PIN_1   /* 模块 DC  (第6脚) -> PC1 */
#define PIN_RES  GPIO_PIN_0   /* 模块 RST (第5脚) -> PC0 */

static void delay_ms(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 40000; j++)
            ;
}

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

/* ---- SPI0：模式0 / MSB / 8bit / 软件NSS / 慢速 ---- */
static void spi_send_byte(uint8_t b) {
    while (RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE))
        ;
    spi_i2s_data_transmit(SPI0, (uint16_t)b);
    while (SET == spi_i2s_flag_get(SPI0, SPI_FLAG_TRANS))
        ; /* GD32 真名，非 BSY */
}
static void spi0_init(void) {
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

/* ---- ST7735S 原语 ---- */
static void lcd_cmd(uint8_t c) {
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_reset(GPIOC, PIN_DC);
    spi_send_byte(c);
    gpio_bit_set(GPIOC, PIN_CS);
}
static void lcd_data1(uint8_t d) {
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_DC);
    spi_send_byte(d);
    gpio_bit_set(GPIOC, PIN_CS);
}
static void lcd_dataN(const uint8_t *p, uint16_t n) {
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_DC);
    while (n--)
        spi_send_byte(*p++);
    gpio_bit_set(GPIOC, PIN_CS);
}
static void lcd_reset(void) {
    gpio_bit_set(GPIOC, PIN_RES);
    delay_ms(10);
    gpio_bit_reset(GPIOC, PIN_RES);
    delay_ms(20);
    gpio_bit_set(GPIOC, PIN_RES);
    delay_ms(120);
}
static void lcd_init(void) {
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
static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
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
static void fill_solid(uint32_t count, uint16_t color) {
    uint8_t hi = color >> 8, lo = color & 0xFF;
    gpio_bit_reset(GPIOC, PIN_CS);
    gpio_bit_set(GPIOC, PIN_DC);
    while (count--) {
        spi_send_byte(hi);
        spi_send_byte(lo);
    }
    gpio_bit_set(GPIOC, PIN_CS);
}
static void fill_screen(uint16_t color) {
    set_window(0, 0, LCD_W - 1, LCD_H - 1);
    fill_solid((uint32_t)LCD_W * LCD_H, color);
}

int main(void) {
    usart0_init();
    u_puts("\r\n>>>>>>>>>> ALIVE: program running, SPI0 init start "
           "<<<<<<<<<<\r\n");
    u_puts("if you see this but screen stays white -> waveform not reaching "
           "panel.\r\n");
    spi0_init();
    u_puts("spi0 init done. lcd init...\r\n");
    lcd_init();
    u_puts("lcd init done. fill BLACK -> screen MUST turn BLACK (white->black "
           "proves fill works).\r\n");
    fill_screen(C_BLACK);
    delay_ms(1500);

    u_puts("now loop R/G/B ~1s each. watch serial vs screen.\r\n");
    while (1) {
        fill_screen(C_RED);
        u_puts("RED\r\n");
        delay_ms(1000);
        fill_screen(C_GREEN);
        u_puts("GREEN\r\n");
        delay_ms(1000);
        fill_screen(C_BLUE);
        u_puts("BLUE\r\n");
        delay_ms(1000);
    }
}