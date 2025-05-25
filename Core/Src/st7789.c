// st7789.c - ST7789 TFT显示屏驱动实现
#include "st7789.h"
#include <string.h>
#include <stdlib.h>

// 完整的5x7字体数据
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,   // 20 空格
    0x00, 0x00, 0x5F, 0x00, 0x00,   // 21 !
    0x00, 0x07, 0x00, 0x07, 0x00,   // 22 "
    0x14, 0x7F, 0x14, 0x7F, 0x14,   // 23 #
    0x24, 0x2A, 0x7F, 0x2A, 0x12,   // 24 $
    0x23, 0x13, 0x08, 0x64, 0x62,   // 25 %
    0x36, 0x49, 0x55, 0x22, 0x50,   // 26 &
    0x00, 0x05, 0x03, 0x00, 0x00,   // 27 '
    0x00, 0x1C, 0x22, 0x41, 0x00,   // 28 (
    0x00, 0x41, 0x22, 0x1C, 0x00,   // 29 )
    0x08, 0x2A, 0x1C, 0x2A, 0x08,   // 2A *
    0x08, 0x08, 0x3E, 0x08, 0x08,   // 2B +
    0x00, 0x50, 0x30, 0x00, 0x00,   // 2C ,
    0x08, 0x08, 0x08, 0x08, 0x08,   // 2D -
    0x00, 0x60, 0x60, 0x00, 0x00,   // 2E .
    0x20, 0x10, 0x08, 0x04, 0x02,   // 2F /
    0x3E, 0x51, 0x49, 0x45, 0x3E,   // 30 0
    0x00, 0x42, 0x7F, 0x40, 0x00,   // 31 1
    0x42, 0x61, 0x51, 0x49, 0x46,   // 32 2
    0x21, 0x41, 0x45, 0x4B, 0x31,   // 33 3
    0x18, 0x14, 0x12, 0x7F, 0x10,   // 34 4
    0x27, 0x45, 0x45, 0x45, 0x39,   // 35 5
    0x3C, 0x4A, 0x49, 0x49, 0x30,   // 36 6
    0x01, 0x71, 0x09, 0x05, 0x03,   // 37 7
    0x36, 0x49, 0x49, 0x49, 0x36,   // 38 8
    0x06, 0x49, 0x49, 0x29, 0x1E,   // 39 9
    0x00, 0x36, 0x36, 0x00, 0x00,   // 3A :
    0x00, 0x56, 0x36, 0x00, 0x00,   // 3B ;
    0x00, 0x08, 0x14, 0x22, 0x41,   // 3C 
    0x14, 0x14, 0x14, 0x14, 0x14,   // 3D =
    0x41, 0x22, 0x14, 0x08, 0x00,   // 3E >
    0x02, 0x01, 0x51, 0x09, 0x06,   // 3F ?
    0x32, 0x49, 0x79, 0x41, 0x3E,   // 40 @
    0x7E, 0x11, 0x11, 0x11, 0x7E,   // 41 A
    0x7F, 0x49, 0x49, 0x49, 0x36,   // 42 B
    0x3E, 0x41, 0x41, 0x41, 0x22,   // 43 C
    0x7F, 0x41, 0x41, 0x22, 0x1C,   // 44 D
    0x7F, 0x49, 0x49, 0x49, 0x41,   // 45 E
    0x7F, 0x09, 0x09, 0x01, 0x01,   // 46 F
    0x3E, 0x41, 0x41, 0x49, 0x7A,   // 47 G
    0x7F, 0x08, 0x08, 0x08, 0x7F,   // 48 H
    0x00, 0x41, 0x7F, 0x41, 0x00,   // 49 I
    0x20, 0x40, 0x41, 0x3F, 0x01,   // 4A J
    0x7F, 0x08, 0x14, 0x22, 0x41,   // 4B K
    0x7F, 0x40, 0x40, 0x40, 0x40,   // 4C L
    0x7F, 0x02, 0x04, 0x02, 0x7F,   // 4D M
    0x7F, 0x04, 0x08, 0x10, 0x7F,   // 4E N
    0x3E, 0x41, 0x41, 0x41, 0x3E,   // 4F O
    0x7F, 0x09, 0x09, 0x09, 0x06,   // 50 P
    0x3E, 0x41, 0x51, 0x21, 0x5E,   // 51 Q
    0x7F, 0x09, 0x19, 0x29, 0x46,   // 52 R
    0x46, 0x49, 0x49, 0x49, 0x31,   // 53 S
    0x01, 0x01, 0x7F, 0x01, 0x01,   // 54 T
    0x3F, 0x40, 0x40, 0x40, 0x3F,   // 55 U
    0x1F, 0x20, 0x40, 0x20, 0x1F,   // 56 V
    0x3F, 0x40, 0x38, 0x40, 0x3F,   // 57 W
    0x63, 0x14, 0x08, 0x14, 0x63,   // 58 X
    0x07, 0x08, 0x70, 0x08, 0x07,   // 59 Y
    0x61, 0x51, 0x49, 0x45, 0x43,   // 5A Z
    0x00, 0x7F, 0x41, 0x41, 0x00,   // 5B [
    0x02, 0x04, 0x08, 0x10, 0x20,   // 5C \
    0x00, 0x41, 0x41, 0x7F, 0x00,   // 5D ]
    0x04, 0x02, 0x01, 0x02, 0x04,   // 5E ^
    0x40, 0x40, 0x40, 0x40, 0x40,   // 5F _
    0x00, 0x01, 0x02, 0x04, 0x00,   // 60 `
    0x20, 0x54, 0x54, 0x54, 0x78,   // 61 a
    0x7F, 0x48, 0x44, 0x44, 0x38,   // 62 b
    0x38, 0x44, 0x44, 0x44, 0x20,   // 63 c
    0x38, 0x44, 0x44, 0x48, 0x7F,   // 64 d
    0x38, 0x54, 0x54, 0x54, 0x18,   // 65 e
    0x08, 0x7E, 0x09, 0x01, 0x02,   // 66 f
    0x08, 0x14, 0x54, 0x54, 0x3C,   // 67 g
    0x7F, 0x08, 0x04, 0x04, 0x78,   // 68 h
    0x00, 0x44, 0x7D, 0x40, 0x00,   // 69 i
    0x20, 0x40, 0x44, 0x3D, 0x00,   // 6A j
    0x7F, 0x10, 0x28, 0x44, 0x00,   // 6B k
    0x00, 0x41, 0x7F, 0x40, 0x00,   // 6C l
    0x7C, 0x04, 0x18, 0x04, 0x78,   // 6D m
    0x7C, 0x08, 0x04, 0x04, 0x78,   // 6E n
    0x38, 0x44, 0x44, 0x44, 0x38,   // 6F o
    0x7C, 0x14, 0x14, 0x14, 0x08,   // 70 p
    0x08, 0x14, 0x14, 0x18, 0x7C,   // 71 q
    0x7C, 0x08, 0x04, 0x04, 0x08,   // 72 r
    0x48, 0x54, 0x54, 0x54, 0x20,   // 73 s
    0x04, 0x3F, 0x44, 0x40, 0x20,   // 74 t
    0x3C, 0x40, 0x40, 0x20, 0x7C,   // 75 u
    0x1C, 0x20, 0x40, 0x20, 0x1C,   // 76 v
    0x3C, 0x40, 0x30, 0x40, 0x3C,   // 77 w
    0x44, 0x28, 0x10, 0x28, 0x44,   // 78 x
    0x0C, 0x50, 0x50, 0x50, 0x3C,   // 79 y
    0x44, 0x64, 0x54, 0x4C, 0x44,   // 7A z
    0x00, 0x08, 0x36, 0x41, 0x00,   // 7B {
    0x00, 0x00, 0x7F, 0x00, 0x00,   // 7C |
    0x00, 0x41, 0x36, 0x08, 0x00,   // 7D }
    0x08, 0x08, 0x2A, 0x1C, 0x08,   // 7E ~
    0x08, 0x1C, 0x2A, 0x08, 0x08    // 7F DEL
};

// 旋转方向
static uint8_t _rotation = 0;
static uint16_t _width = ST7789_WIDTH;
static uint16_t _height = ST7789_HEIGHT;

// 延时函数
static void ST7789_Delay(uint32_t ms) {
    HAL_Delay(ms);
}

// 短延时，不使用HAL_Delay以减少开销
static inline void short_delay(void) {
    for(volatile int i = 0; i < 10; i++);
}

// 软件SPI写一个字节 - 优化时序
static void ST7789_Write8(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        HAL_GPIO_WritePin(ST7789_SCL_PORT, ST7789_SCL_PIN, GPIO_PIN_RESET);
        short_delay(); // 短延时确保信号稳定
        
        HAL_GPIO_WritePin(ST7789_SDA_PORT, ST7789_SDA_PIN, 
                         (data & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        short_delay(); // 短延时确保信号稳定
        
        HAL_GPIO_WritePin(ST7789_SCL_PORT, ST7789_SCL_PIN, GPIO_PIN_SET);
        short_delay(); // 短延时确保信号稳定
    }
}

// 写命令
static void ST7789_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_RESET);
    ST7789_Write8(cmd);
}

// 写数据
static void ST7789_WriteData(uint8_t data) {
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    ST7789_Write8(data);
}

// 写16位数据
static void ST7789_WriteData16(uint16_t data) {
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    ST7789_Write8(data >> 8);
    ST7789_Write8(data & 0xFF);
}

// 初始化ST7789 - 重点优化
void ST7789_Init(void) {
    // 初始化GPIO
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能时钟
    __HAL_RCC_GPIOF_CLK_ENABLE();
    
    // 配置所有引脚为输出
    GPIO_InitStruct.Pin = ST7789_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 使用上拉电阻
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ST7789_SCL_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = ST7789_SDA_PIN;
    HAL_GPIO_Init(ST7789_SDA_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = ST7789_DC_PIN;
    HAL_GPIO_Init(ST7789_DC_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = ST7789_RES_PIN;
    HAL_GPIO_Init(ST7789_RES_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = ST7789_BLK_PIN;
    HAL_GPIO_Init(ST7789_BLK_PORT, &GPIO_InitStruct);
    
    // 初始设置所有引脚为高电平
    HAL_GPIO_WritePin(ST7789_SCL_PORT, ST7789_SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_SDA_PORT, ST7789_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_RES_PORT, ST7789_RES_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_BLK_PORT, ST7789_BLK_PIN, GPIO_PIN_RESET); // 先关闭背光
    
    // 硬件复位 - 增加复位时间
    HAL_GPIO_WritePin(ST7789_RES_PORT, ST7789_RES_PIN, GPIO_PIN_SET);
    ST7789_Delay(50);
    HAL_GPIO_WritePin(ST7789_RES_PORT, ST7789_RES_PIN, GPIO_PIN_RESET);
    ST7789_Delay(50);
    HAL_GPIO_WritePin(ST7789_RES_PORT, ST7789_RES_PIN, GPIO_PIN_SET);
    ST7789_Delay(150);
    
    // 初始化显示 - 强化版初始化序列
    ST7789_WriteCommand(ST7789_SWRESET); // 软件复位
    ST7789_Delay(150);
    
    ST7789_WriteCommand(ST7789_SLPOUT); // 退出睡眠模式
    ST7789_Delay(150);
    
    // 添加电源控制命令
    ST7789_WriteCommand(0xB2); // 帧率控制
    ST7789_WriteData(0x0C);
    ST7789_WriteData(0x0C);
    ST7789_WriteData(0x00);
    ST7789_WriteData(0x33);
    ST7789_WriteData(0x33);
    
    ST7789_WriteCommand(0xB7); // 栅极控制
    ST7789_WriteData(0x35);
    
    // 对比度设置
    ST7789_WriteCommand(0xC0); // 电源控制1
    ST7789_WriteData(0x2C);
    
    ST7789_WriteCommand(0xC3); // 电源控制2
    ST7789_WriteData(0x0A);
    
    ST7789_WriteCommand(0xC4); // 电源控制3
    ST7789_WriteData(0x20);
    
    ST7789_WriteCommand(0xC6); // VCOM控制
    ST7789_WriteData(0x0F);
    
    ST7789_WriteCommand(0xBB); // VCOMS设置
    ST7789_WriteData(0x20);
    
    ST7789_WriteCommand(ST7789_COLMOD); // 设置色彩模式
    ST7789_WriteData(0x55); // 16位/像素
    
    ST7789_WriteCommand(ST7789_MADCTL); // 存储器访问控制
    ST7789_WriteData(0x00); // 默认方向
    
    // 设置显示区域
    ST7789_WriteCommand(ST7789_CASET); // 列地址设置
    ST7789_WriteData(0x00);
    ST7789_WriteData(0x00);
    ST7789_WriteData(0x00);
    ST7789_WriteData(0xEF); // 239
    
    ST7789_WriteCommand(ST7789_RASET); // 行地址设置
    ST7789_WriteData(0x00);
    ST7789_WriteData(0x00);
    ST7789_WriteData(0x00);
    ST7789_WriteData(0xEF); // 239
    
    ST7789_WriteCommand(0xE0); // 正伽马校正
    ST7789_WriteData(0xD0);
    ST7789_WriteData(0x04);
    ST7789_WriteData(0x0D);
    ST7789_WriteData(0x11);
    ST7789_WriteData(0x13);
    ST7789_WriteData(0x2B);
    ST7789_WriteData(0x3F);
    ST7789_WriteData(0x54);
    ST7789_WriteData(0x4C);
    ST7789_WriteData(0x18);
    ST7789_WriteData(0x0D);
    ST7789_WriteData(0x0B);
    ST7789_WriteData(0x1F);
    ST7789_WriteData(0x23);
    
    ST7789_WriteCommand(0xE1); // 负伽马校正
    ST7789_WriteData(0xD0);
    ST7789_WriteData(0x04);
    ST7789_WriteData(0x0C);
    ST7789_WriteData(0x11);
    ST7789_WriteData(0x13);
    ST7789_WriteData(0x2C);
    ST7789_WriteData(0x3F);
    ST7789_WriteData(0x44);
    ST7789_WriteData(0x51);
    ST7789_WriteData(0x2F);
    ST7789_WriteData(0x1F);
    ST7789_WriteData(0x1F);
    ST7789_WriteData(0x20);
    ST7789_WriteData(0x23);
    
    ST7789_WriteCommand(ST7789_INVON); // 反显
    ST7789_Delay(10);
    
    ST7789_WriteCommand(ST7789_NORON); // 设置正常显示模式
    ST7789_Delay(10);
    
    ST7789_WriteCommand(ST7789_DISPON); // 打开显示
    ST7789_Delay(150);
    
    // 确保背光打开
    HAL_GPIO_WritePin(ST7789_BLK_PORT, ST7789_BLK_PIN, GPIO_PIN_SET);
    ST7789_Delay(100);
    
    // 默认填充黑色屏幕
    ST7789_FillScreen(BLACK);
}

// 设置旋转
void ST7789_SetRotation(uint8_t rotation) {
    _rotation = rotation % 4;
    
    ST7789_WriteCommand(ST7789_MADCTL);
    
    switch (_rotation) {
        case 0:  // 0度旋转
            ST7789_WriteData(0x00);
            _width = ST7789_WIDTH;
            _height = ST7789_HEIGHT;
            break;
        case 1:  // 90度旋转
            ST7789_WriteData(0x60);
            _width = ST7789_HEIGHT;
            _height = ST7789_WIDTH;
            break;
        case 2:  // 180度旋转
            ST7789_WriteData(0xC0);
            _width = ST7789_WIDTH;
            _height = ST7789_HEIGHT;
            break;
        case 3:  // 270度旋转
            ST7789_WriteData(0xA0);
            _width = ST7789_HEIGHT;
            _height = ST7789_WIDTH;
            break;
    }
}

// 设置绘图窗口
void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ST7789_WriteCommand(ST7789_CASET);    // 列地址设置
    ST7789_WriteData16(x0);
    ST7789_WriteData16(x1);
    
    ST7789_WriteCommand(ST7789_RASET);    // 行地址设置
    ST7789_WriteData16(y0);
    ST7789_WriteData16(y1);
    
    ST7789_WriteCommand(ST7789_RAMWR);    // 开始写入
}

// 填充屏幕为指定色彩
void ST7789_FillScreen(uint16_t color) {
    ST7789_FillRectangle(0, 0, _width, _height, color);
}

// 画一个像素点
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= _width || y >= _height) return;
    
    ST7789_SetWindow(x, y, x, y);
    ST7789_WriteData16(color);
}

// 画一条线
void ST7789_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    
    if (steep) {
        uint16_t tmp;
        tmp = x0; x0 = y0; y0 = tmp;
        tmp = x1; x1 = y1; y1 = tmp;
    }
    
    if (x0 > x1) {
        uint16_t tmp;
        tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }
    
    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep;
    
    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }
    
    for (; x0 <= x1; x0++) {
        if (steep) {
            ST7789_DrawPixel(y0, x0, color);
        } else {
            ST7789_DrawPixel(x0, y0, color);
        }
        
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

// 画一个矩形
void ST7789_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    ST7789_DrawLine(x, y, x + w - 1, y, color);
    ST7789_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    ST7789_DrawLine(x, y, x, y + h - 1, color);
    ST7789_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

// 填充矩形 - 优化批量填充
void ST7789_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= _width || y >= _height) return;
    
    if ((x + w - 1) >= _width) w = _width - x;
    if ((y + h - 1) >= _height) h = _height - y;
    
    ST7789_SetWindow(x, y, x + w - 1, y + h - 1);
    
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    
    // 批量处理以提高效率
    uint32_t pixelCount = (uint32_t)w * h;
    for (uint32_t i = 0; i < pixelCount; i++) {
        ST7789_Write8(color >> 8);
        ST7789_Write8(color & 0xFF);
    }
}

// 精确的字符映射函数 - 修复显示问题
void ST7789_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t size) {
    // 创建详细的字符映射表 - 基于错误模式分析
    char fixed_ch = ch;
    
    // 从观察到的结果来看，小写字母的映射存在偏移问题
    if (ch >= 'a' && ch <= 'z') {
        // 小写字母'a'需要特殊处理 - 显示为'`' (ASCII 96)的下一个字符
        if (ch == 'a') {
            fixed_ch = '`';  // 显示为'a'
        }
        // 小写字母'z'需要特殊处理，避免显示为'{'
        else if (ch == 'z') {
            fixed_ch = 'y';  // 显示为'z'
        }
        // 其他小写字母需要减1偏移
        else {
            fixed_ch = ch - 1;  // 例如，'b'显示为'a'，'c'显示为'b'，以此类推
        }
    }
    
    // 进行边界检查，确保字符在有效范围内
    if (fixed_ch < ' ' || fixed_ch > '~') {
        fixed_ch = '?';  // 如果超出范围，显示'?'
    }
    
    // 检查绘制边界
    if ((x >= _width) || (y >= _height) || 
        ((x + 6 * size - 1) < 0) || ((y + 8 * size - 1) < 0))
        return;
    
    // 绘制修正后的字符
    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = font5x7[(fixed_ch - ' ') * 5 + i];
        
        for (int8_t j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                if (size == 1) {
                    ST7789_DrawPixel(x + i, y + j, color);
                } else {
                    ST7789_FillRectangle(x + i * size, y + j * size, size, size, color);
                }
            } else if (bg != color) {
                if (size == 1) {
                    ST7789_DrawPixel(x + i, y + j, bg);
                } else {
                    ST7789_FillRectangle(x + i * size, y + j * size, size, size, bg);
                }
            }
        }
    }
}

// 画字符串
void ST7789_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    uint16_t startX = x;
    
    while (*str) {
        if (*str == '\n') {
            y += size * 8;
            x = startX;
        } else {
            ST7789_DrawChar(x, y, *str, color, bg, size);
            x += size * 6;
            
            // 处理换行
            if (x > _width - size * 6) {
                y += size * 8;
                x = startX;
            }
        }
        str++;
    }
}

// 写一个字符 - 这是带背景的变体
void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t size) {
    ST7789_DrawChar(x, y, ch, color, bg, size);
}

// 写字符串 - 带背景
void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    ST7789_DrawString(x, y, str, color, bg, size);
}

// 画圆
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    ST7789_DrawPixel(x0, y0 + r, color);
    ST7789_DrawPixel(x0, y0 - r, color);
    ST7789_DrawPixel(x0 + r, y0, color);
    ST7789_DrawPixel(x0 - r, y0, color);
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        ST7789_DrawPixel(x0 + x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color);
        ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 + y, y0 + x, color);
        ST7789_DrawPixel(x0 - y, y0 + x, color);
        ST7789_DrawPixel(x0 + y, y0 - x, color);
        ST7789_DrawPixel(x0 - y, y0 - x, color);
    }
}

// 填充圆
void ST7789_FillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    for (int16_t i = -r; i <= r; i++) {
        ST7789_DrawPixel(x0, y0 + i, color);
    }
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        for (int16_t i = -y; i <= y; i++) {
            ST7789_DrawPixel(x0 + x, y0 + i, color);
            ST7789_DrawPixel(x0 - x, y0 + i, color);
        }
        for (int16_t i = -x; i <= x; i++) {
            ST7789_DrawPixel(x0 + i, y0 + y, color);
            ST7789_DrawPixel(x0 + i, y0 - y, color);
        }
    }
}

// 绘制图像
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data) {
    if ((x >= _width) || (y >= _height)) return;
    
    if ((x + w - 1) >= _width) w = _width - x;
    if ((y + h - 1) >= _height) h = _height - y;
    
    ST7789_SetWindow(x, y, x + w - 1, y + h - 1);
    
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    
    for (uint32_t i = 0; i < w * h; i++) {
        ST7789_WriteData16(data[i]);
    }
}

/**
  * @brief  在屏幕上绘制RGB565格式字节数组图像
  * @param  x: X坐标起点
  * @param  y: Y坐标起点
  * @param  width: 图像宽度
  * @param  height: 图像高度
  * @param  image: RGB565图像数据（字节数组形式）
  */
void ST7789_DrawRGB565Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* image) {
    uint32_t i = 0;
    
    if (x + width > _width || y + height > _height || !image) {
        return;
    }
    
    // 设置显示区域
    ST7789_SetWindow(x, y, x + width - 1, y + height - 1);
    
    HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    
    // 发送图像数据 - 字节数组形式
    for (i = 0; i < width * height * 2; i += 2) {
        // 发送高字节和低字节
        ST7789_Write8(image[i]);
        ST7789_Write8(image[i + 1]);
    }
}