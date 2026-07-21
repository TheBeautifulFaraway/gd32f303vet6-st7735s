#pragma once

#include "gd32f30x.h"
#include "stdint.h"

//================================脚================================
// RST->PC0, DC->PC1, CS->PC2
#define PIN_CS GPIO_PIN_2  /* 模块 CS  (第7脚) -> PC2 */
#define PIN_DC GPIO_PIN_1  /* 模块 DC  (第6脚) -> PC1 */
#define PIN_RES GPIO_PIN_0 /* 模块 RST (第5脚) -> PC0 */

//================================st7735s参数================================
// 分辨率
#define LCD_W 128
#define LCD_H 160

//================================函数声明================================
// 初始化spi和lcd硬件
void lcd_init(void);

// 发送单个字节
void spi_send_byte(uint8_t b);

// 批量发送像素数据
void lcd_send_pixels(uint16_t *color_p, uint32_t len);

// 设置显示窗口
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
