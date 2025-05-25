// st7789.h - ST7789 TFT��ʾ������
#ifndef ST7789_H
#define ST7789_H

#include "main.h"
#include <stdint.h>
#include <stdlib.h>

// ST7789��ʾ���ߴ�
#define ST7789_WIDTH  240
#define ST7789_HEIGHT 240

// ���Ŷ��� - �������Ӳ��
#define ST7789_SCL_PORT  GPIOF  // SCL (ʱ����)
#define ST7789_SCL_PIN   GPIO_PIN_0
#define ST7789_SDA_PORT  GPIOF  // SDA (������)
#define ST7789_SDA_PIN   GPIO_PIN_1
#define ST7789_RES_PORT  GPIOF  // RES (��λ��)
#define ST7789_RES_PIN   GPIO_PIN_2
#define ST7789_DC_PORT   GPIOF  // DC (����/�������)
#define ST7789_DC_PIN    GPIO_PIN_3
#define ST7789_BLK_PORT  GPIOF  // BLK (�������)
#define ST7789_BLK_PIN   GPIO_PIN_5

// ɫ�ʶ��� (RGB565��ʽ)
#define BLACK       0x0000
#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define PURPLE      0x780F
#define OLIVE       0x7BE0
#define LIGHTGREY   0xC618
#define DARKGREY    0x7BEF
#define BLUE        0x001F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define WHITE       0xFFFF
#define ORANGE      0xFD20

// ST7789����
#define ST7789_NOP      0x00
#define ST7789_SWRESET  0x01
#define ST7789_SLPIN    0x10
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_COLMOD   0x3A
#define ST7789_MADCTL   0x36

// ��������
void ST7789_Init(void);
void ST7789_SetRotation(uint8_t rotation);
void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ST7789_FillScreen(uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void ST7789_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void ST7789_FillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void ST7789_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t size);
void ST7789_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t size);
void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

// ������������ - RGB565�ֽ�����ͼ����ʾ
void ST7789_DrawRGB565Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* image);

#endif /* ST7789_H */