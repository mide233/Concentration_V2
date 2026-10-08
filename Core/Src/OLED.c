#include "OLED.h"
#include <string.h>

static uint8_t logic_buffer[512]; // 32x128 图形缓冲区
static uint8_t phys_buffer[512];  // 128x32 物理缓冲区
static uint8_t dot_exist = 0;

#define TOP_START_Y 0
#define MID_START_Y TOP_AREA_HEIGHT
#define BOTTOM_START_Y (TOP_AREA_HEIGHT + MID_AREA_HEIGHT)

// I2C 通信
static void I2C_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 100);
}

static void I2C_WriteData(uint8_t data)
{
    uint8_t buf[2] = {0x40, data};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 100);
}

// 图形绘制（逻辑坐标系 32x128）
static void Logic_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_LOGIC_WIDTH || y >= OLED_LOGIC_HEIGHT)
        return;
    uint16_t page = y / 8;
    uint16_t col = x;
    uint8_t bit = 1 << (y % 8);
    if (color)
        logic_buffer[page * OLED_LOGIC_WIDTH + col] |= bit;
    else
        logic_buffer[page * OLED_LOGIC_WIDTH + col] &= ~bit;
}

static void Logic_FillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    for (uint8_t y = y1; y <= y2; y++)
        for (uint8_t x = x1; x <= x2; x++)
            Logic_DrawPixel(x, y, color);
}

static void Logic_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    for (uint8_t x = x1; x <= x2; x++)
    {
        Logic_DrawPixel(x, y1, color);
        Logic_DrawPixel(x, y2, color);
    }
    for (uint8_t y = y1 + 1; y < y2; y++)
    {
        Logic_DrawPixel(x1, y, color);
        Logic_DrawPixel(x2, y, color);
    }
}

// 电池（宽24高12）
static void Logic_DrawBattery(uint8_t x, uint8_t y, uint8_t level)
{
    Logic_DrawRect(x, y, x + 22, y + 11, 1);
    Logic_FillRect(x + 23, y + 3, x + 24, y + 8, 1);
    uint8_t seg_width = 4;
    for (uint8_t i = 0; i < level; i++)
    {
        uint8_t ix = x + 2 + i * seg_width;
        if (ix + 2 <= x + 20)
            Logic_FillRect(ix, y + 2, ix + 2, y + 9, 1);
    }
}

// 蓝牙图标 16x20
static const uint8_t bluetooth_16x20[20][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
//// 蓝牙图标 16x20
// static const uint8_t bluetooth_16x20[20][16] = {
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
//     {0,0,0,0,0,1,0,1,0,1,0,0,0,0,0,0},
//     {0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,0},
//     {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
//     {0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,0},
//     {0,0,0,0,0,1,0,1,0,1,0,0,0,0,0,0},
//     {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
// };
static void Logic_DrawBluetooth(uint8_t x, uint8_t y, uint8_t show)
{
    if (!show)
        return;
    for (uint8_t i = 0; i < 20; i++)
        for (uint8_t j = 0; j < 16; j++)
            if (bluetooth_16x20[i][j])
                Logic_DrawPixel(x + j, y + i, 1);
}

static void Logic_DrawDot(uint8_t x, uint8_t y, uint8_t exist)
{
    if (!exist)
        return;
    for (int8_t dy = -2; dy <= 2; dy++)
        for (int8_t dx = -2; dx <= 2; dx++)
            if (dx * dx + dy * dy <= 4)
                Logic_DrawPixel(x + dx, y + dy, 1);
}

// 进度条（从下向上填充）
static void Logic_DrawProgressBar(uint8_t percent, uint8_t start_y, uint8_t area_height)
{
    uint8_t bar_height = area_height - 20;
    if (bar_height < 8)
        bar_height = 8;
    uint8_t bar_width = 28;
    uint8_t start_x = (OLED_LOGIC_WIDTH - bar_width) / 2;
    Logic_DrawRect(start_x, start_y, start_x + bar_width - 1, start_y + bar_height - 1, 1);
    uint8_t inner_x1 = start_x + 5;
    uint8_t inner_x2 = start_x + bar_width - 1 - 5;
    uint8_t inner_y1 = start_y + 5;
    uint8_t inner_y2 = start_y + bar_height - 1 - 5;
    if (inner_x1 > inner_x2 || inner_y1 > inner_y2)
        return;
    uint8_t fill_height = (percent * (inner_y2 - inner_y1 + 1) + 50) / 100;
    if (fill_height > (inner_y2 - inner_y1 + 1))
        fill_height = inner_y2 - inner_y1 + 1;
    if (fill_height > 0)
    {
        uint8_t fill_start_y = inner_y2 - fill_height + 1;
        Logic_FillRect(inner_x1, fill_start_y, inner_x2, inner_y2, 1);
    }
}

// 软件旋转：32x128 顺时针90度 -> 128x32
static void RotateLogicToPhysical(void)
{
    memset(phys_buffer, 0, sizeof(phys_buffer));
    for (uint8_t ly = 0; ly < OLED_LOGIC_HEIGHT; ly++)
    {
        for (uint8_t lx = 0; lx < OLED_LOGIC_WIDTH; lx++)
        {
            uint8_t color = (logic_buffer[(ly / 8) * OLED_LOGIC_WIDTH + lx] >> (ly % 8)) & 0x01;
            if (color)
            {
                uint8_t px = ly;
                uint8_t py = OLED_LOGIC_WIDTH - 1 - lx;
                if (px < OLED_PHYS_WIDTH && py < OLED_PHYS_HEIGHT)
                {
                    uint16_t page = py / 8;
                    uint8_t bit = 1 << (py % 8);
                    phys_buffer[page * OLED_PHYS_WIDTH + px] |= bit;
                }
            }
        }
    }
}

// ========== 字体（8x16，正立）==========
static const uint8_t font8x16[][16] = {
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // 0
    {0x00, 0x00, 0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00}, // 1
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x42, 0x7E, 0x00, 0x00, 0x00}, // 2
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x02, 0x0C, 0x02, 0x02, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // 3
    {0x00, 0x00, 0x04, 0x0C, 0x14, 0x24, 0x44, 0x84, 0x7E, 0x04, 0x04, 0x04, 0x1F, 0x00, 0x00, 0x00}, // 4
    {0x00, 0x00, 0x7E, 0x40, 0x40, 0x40, 0x78, 0x44, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // 5
    {0x00, 0x00, 0x3C, 0x42, 0x40, 0x40, 0x78, 0x44, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // 6
    {0x00, 0x00, 0x7E, 0x42, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00}, // 7
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // 8
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3E, 0x02, 0x02, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // 9
    {0x00, 0x00, 0x60, 0x90, 0x90, 0x60, 0x08, 0x14, 0x12, 0x21, 0x41, 0x82, 0x7C, 0x00, 0x00, 0x00}, // %
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00}, // O
    {0x00, 0x00, 0x42, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00}, // K
    {0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // !
};

// 绘制正立字符（不旋转）
static void Phys_DrawCharNormal(uint8_t x, uint8_t y, char ch, uint8_t color)
{
    if (x + 8 > OLED_PHYS_WIDTH || y + 16 > OLED_PHYS_HEIGHT)
        return;
    uint8_t idx = 255;
    if (ch >= '0' && ch <= '9')
        idx = ch - '0';
    else if (ch == '%')
        idx = 10;
    else if (ch == 'O')
        idx = 11;
    else if (ch == 'K')
        idx = 12;
    else if (ch == '!')
        idx = 13;
    else
        return;
    for (uint8_t i = 0; i < 16; i++)
    {
        uint8_t line = font8x16[idx][i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (line & (1 << j))
            {
                uint8_t px = x + j;
                uint8_t py = y + i;
                if (px < OLED_PHYS_WIDTH && py < OLED_PHYS_HEIGHT)
                {
                    uint16_t page = py / 8;
                    uint8_t bit = 1 << (py % 8);
                    if (color)
                        phys_buffer[page * OLED_PHYS_WIDTH + px] |= bit;
                    else
                        phys_buffer[page * OLED_PHYS_WIDTH + px] &= ~bit;
                }
            }
        }
    }
}

// 绘制侧躺字符（顺时针旋转90度，加入镜像调整）
static void Phys_DrawCharRotated(uint8_t x, uint8_t y, char ch, uint8_t color)
{
    if (x + 16 > OLED_PHYS_WIDTH || y + 8 > OLED_PHYS_HEIGHT)
        return;
    uint8_t idx = 255;
    if (ch >= '0' && ch <= '9')
        idx = ch - '0';
    else if (ch == '%')
        idx = 10;
    else if (ch == 'O')
        idx = 11;
    else if (ch == 'K')
        idx = 12;
    else if (ch == '!')
        idx = 13;
    else
        return;

    for (uint8_t i = 0; i < 16; i++)
    {
        uint8_t line = font8x16[idx][i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (line & (1 << j))
            {
                int16_t nx = x + i;
                int16_t ny = y + (7 - j);
#if TEXT_MIRROR_H
                nx = x + (15 - i);
#endif
#if TEXT_MIRROR_V
                ny = y + j;
#endif
                if (nx >= 0 && nx < OLED_PHYS_WIDTH && ny >= 0 && ny < OLED_PHYS_HEIGHT)
                {
                    uint16_t page = ny / 8;
                    uint8_t bit = 1 << (ny % 8);
                    if (color)
                        phys_buffer[page * OLED_PHYS_WIDTH + nx] |= bit;
                    else
                        phys_buffer[page * OLED_PHYS_WIDTH + nx] &= ~bit;
                }
            }
        }
    }
}

// 水平绘制正立字符串（模式1）
static void Phys_DrawHorizontalString(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    uint8_t x_offset = 0;
    while (*str)
    {
        Phys_DrawCharNormal(x + x_offset, y, *str++, color);
        x_offset += TEXT_CHAR_SPACING;
        if (x_offset + 8 > OLED_PHYS_WIDTH)
            break;
    }
}

// 竖直绘制侧躺字符串（模式0，每个字符侧躺，从上到下排列）
static void Phys_DrawVerticalRotatedString(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    // 如果需要反转顺序，先复制并反转字符串
    char reversed[5];
    const char *p = str;
#if TEXT_REVERSE_ORDER
    uint8_t len = 0;
    while (p[len])
        len++;
    for (uint8_t i = 0; i < len; i++)
    {
        reversed[i] = p[len - 1 - i];
    }
    reversed[len] = 0;
    p = reversed;
#endif

    uint8_t y_offset = 0;
    while (*p)
    {
        Phys_DrawCharRotated(x, y + y_offset, *p++, color);
        y_offset += TEXT_CHAR_SPACING;
        if (y_offset + 8 > OLED_PHYS_HEIGHT)
            break;
    }
}

// 公共更新函数
void OLED_Update(uint8_t battery_level, uint8_t bluetooth_state, uint8_t percent)
{
    memset(logic_buffer, 0, sizeof(logic_buffer));

    uint8_t top_center_y = TOP_START_Y + TOP_AREA_HEIGHT / 2;
    Logic_DrawBattery(4, top_center_y - 6, battery_level);

    uint8_t mid_center_y = MID_START_Y + MID_AREA_HEIGHT / 2;
    Logic_DrawBluetooth(2, mid_center_y - 10, bluetooth_state);
    Logic_DrawDot(OLED_LOGIC_WIDTH - 5, mid_center_y, dot_exist);

    Logic_DrawProgressBar(percent, BOTTOM_START_Y, BOTTOM_AREA_HEIGHT);

    RotateLogicToPhysical();

    char text[5] = {0};
    if (percent >= 100)
    {
        text[0] = 'O';
        text[1] = 'K';
        text[2] = '!';
    }
    else
    {
        if (percent < 10)
        {
            text[0] = '0' + percent;
            text[1] = '%';
        }
        else
        {
            text[0] = '0' + (percent / 10);
            text[1] = '0' + (percent % 10);
            text[2] = '%';
        }
    }

#if TEXT_MODE == 0
    Phys_DrawVerticalRotatedString(TEXT_POS_X, TEXT_POS_Y, text, 1);
#else
    Phys_DrawHorizontalString(TEXT_POS_X, TEXT_POS_Y, text, 1);
#endif

    OLED_Refresh();
}

void OLED_Set_Dot(uint8_t exist)
{
    dot_exist = exist;
}

void OLED_Clear(void)
{
    memset(logic_buffer, 0, sizeof(logic_buffer));
    memset(phys_buffer, 0, sizeof(phys_buffer));
}

void OLED_Refresh(void)
{
    for (uint8_t page = 0; page < OLED_PHYS_HEIGHT / 8; page++)
    {
        I2C_WriteCmd(0xB0 + page);
        I2C_WriteCmd(0x00);
        I2C_WriteCmd(0x10);
        for (uint8_t col = 0; col < OLED_PHYS_WIDTH; col++)
        {
            I2C_WriteData(phys_buffer[page * OLED_PHYS_WIDTH + col]);
        }
    }
}

void OLED_Init(void)
{
    HAL_Delay(100);
    I2C_WriteCmd(0xAE);
    I2C_WriteCmd(0xD5);
    I2C_WriteCmd(0x80);
    I2C_WriteCmd(0xA8);
    I2C_WriteCmd(0x1F);
    I2C_WriteCmd(0xD3);
    I2C_WriteCmd(0x00);
    I2C_WriteCmd(0x40);
    I2C_WriteCmd(0x8D);
    I2C_WriteCmd(0x14);
    I2C_WriteCmd(0x20);
    I2C_WriteCmd(0x00);
    I2C_WriteCmd(0xA0);
    I2C_WriteCmd(0xC0);
    I2C_WriteCmd(0xDA);
    I2C_WriteCmd(0x02);
    I2C_WriteCmd(0x81);
    I2C_WriteCmd(0x8F);
    I2C_WriteCmd(0xD9);
    I2C_WriteCmd(0xF1);
    I2C_WriteCmd(0xDB);
    I2C_WriteCmd(0x40);
    I2C_WriteCmd(0xA4);
    I2C_WriteCmd(0xA6);
    I2C_WriteCmd(0x2E);
    I2C_WriteCmd(0xAF);

    OLED_Clear();
    OLED_Refresh();
}
