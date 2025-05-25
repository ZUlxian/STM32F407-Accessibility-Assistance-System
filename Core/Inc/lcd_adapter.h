// lcd_adapter.h - 实现LCD与ST7789函数名称的适配
#ifndef LCD_ADAPTER_H
#define LCD_ADAPTER_H

#include "st7789.h"
#include "ov7670.h"  // 包含OV7670头文件

// 定义LCD宽高常量
#define LCD_W ST7789_WIDTH
#define LCD_H ST7789_HEIGHT

// 定义这些外部变量，用于跟踪OV7670状态
extern uint32_t datanum;
extern uint8_t ov_frame;
extern uint8_t ov_rev_ok;

// 函数适配 - 修改为处理不同参数数量
static inline void LCD_ShowString(uint16_t x, uint16_t y, const char *str, 
                                uint16_t color, uint16_t bg, uint8_t size, uint8_t mode) {
    // 忽略最后一个参数mode，它在ST7789驱动中不需要
    ST7789_WriteString(x, y, str, color, bg, size);
}

// 其他适配函数
#define LCD_Init            ST7789_Init
#define LCD_Clear(color)    ST7789_FillScreen(color)
#define LCD_Fill            ST7789_FillRectangle
#define LCD_DrawPoint       ST7789_DrawPixel
#define LCD_DrawLine        ST7789_DrawLine
#define LCD_DrawRectangle   ST7789_DrawRectangle
#define LCD_DrawCircle      ST7789_DrawCircle

// 类似地为ShowChar适配，因为它可能也有同样的参数问题
static inline void LCD_ShowChar(uint16_t x, uint16_t y, char ch, 
                               uint16_t color, uint16_t bg, uint8_t size, uint8_t mode) {
    // 忽略最后一个参数mode
    ST7789_WriteChar(x, y, ch, color, bg, size);
}

// 图片显示适配
static inline void LCD_ShowPicture(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint8_t *image) {
    uint16_t width = x2 - x1;
    uint16_t height = y2 - y1;
    ST7789_DrawRGB565Image(x1, y1, width, height, image);
}

#endif /* LCD_ADAPTER_H */