/*
 * SSD1306 OLED（128x32，I2C1）驱动实现。
 * 绘制流程：先在逻辑缓冲区（32x128）作图，软件旋转到物理缓冲区（128x32），
 *           再按页通过 I2C 写入 OLED。
 */
#include "OLED.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

// 逻辑缓冲（32x128）与物理缓冲（128x32）均按页存放
constexpr size_t kLogicBufferSize = OLED_LOGIC_WIDTH * (OLED_LOGIC_HEIGHT / 8);
constexpr size_t kPhysBufferSize = OLED_PHYS_WIDTH * (OLED_PHYS_HEIGHT / 8);

static_assert(kLogicBufferSize == 512);
static_assert(kPhysBufferSize == 512);
static_assert(TOP_AREA_HEIGHT + MID_AREA_HEIGHT + BOTTOM_AREA_HEIGHT == OLED_LOGIC_HEIGHT);
static_assert(OLED_PHYS_WIDTH == 128);
static_assert(OLED_PHYS_HEIGHT == 32);

// 三个区域的起始 Y 坐标（逻辑坐标系）
constexpr uint8_t TOP_START_Y = 0;
constexpr uint8_t MID_START_Y = TOP_AREA_HEIGHT;
constexpr uint8_t BOTTOM_START_Y = TOP_AREA_HEIGHT + MID_AREA_HEIGHT;

// 蓝牙图标 16x20
constexpr uint8_t bluetooth_16x20[20][16] = {
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
static_assert(sizeof(bluetooth_16x20) / sizeof(bluetooth_16x20[0]) == 20);

// [已注释] 以下为旧版蓝牙图标（16x20）定义，疑似废弃，待人工确认（保留原样，勿删）
//// 蓝牙图标 16x20
// static const uint8_t bluetooth_16x20[20][16] = {
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
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

// ========== 字体表（8x16，正立）==========
constexpr uint8_t font8x16[][16] = {
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
static_assert(sizeof(font8x16) / sizeof(font8x16[0]) == 14);

/*
 * 绘制正立字符（不旋转）；仅支持数字 0-9 及 % O K !
 * 说明：与 Phys_DrawHorizontalString 一同保留为文件内自由函数（接收显式物理缓冲指针），
 *       以便 TEXT_MODE==0 时 GCC 仍能报告 “defined but not used”（AGENTS.md 风险 R6）。
 */
void Phys_DrawCharNormal(uint8_t *phys, uint8_t x, uint8_t y, char ch, uint8_t color)
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
                        phys[page * OLED_PHYS_WIDTH + px] |= bit;
                    else
                        phys[page * OLED_PHYS_WIDTH + px] &= ~bit;
                }
            }
        }
    }
}

// 水平绘制正立字符串（TEXT_MODE==1 时使用）
// 注意：当前 TEXT_MODE==0，本函数未被调用（编译器报 unused），保留待用。
void Phys_DrawHorizontalString(uint8_t *phys, uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    uint8_t x_offset = 0;
    while (*str)
    {
        Phys_DrawCharNormal(phys, x + x_offset, y, *str++, color);
        x_offset += TEXT_CHAR_SPACING;
        if (x_offset + 8 > OLED_PHYS_WIDTH)
            break;
    }
}

/*
 * SSD1306 128x32 显示驱动类。
 * 显示状态全部封装为私有成员；绘制辅助函数为私有成员函数。
 * 文件内单例 g_oled 通过 constinit 静态初始化：不产生 .init_array 动态构造，
 * 构造期不访问 HAL（满足在 HAL_Init() 之前完成静态初始化的约束）。
 * 对外仍暴露原有 OLED_* C 函数（extern "C"，见文件末尾）。
 */
class OledDriver
{
public:
    void init();
    void update(uint8_t battery_level, uint8_t bluetooth_state, uint8_t percent);
    void set_dot(uint8_t exist) { dot_exist = exist; }
    void clear();
    void refresh();

private:
    // I2C 通信
    static void I2C_WriteCmd(uint8_t cmd);
    static void I2C_WriteData(uint8_t data);

    // 图形绘制（逻辑坐标系 32x128）
    void Logic_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
    void Logic_FillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
    void Logic_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
    void Logic_DrawBattery(uint8_t x, uint8_t y, uint8_t level);
    void Logic_DrawBluetooth(uint8_t x, uint8_t y, uint8_t show);
    void Logic_DrawDot(uint8_t x, uint8_t y, uint8_t exist);
    void Logic_DrawProgressBar(uint8_t percent, uint8_t start_y, uint8_t area_height);

    // 软件旋转 + 物理缓冲区文字绘制
    void RotateLogicToPhysical(void);
    void Phys_DrawCharRotated(uint8_t x, uint8_t y, char ch, uint8_t color);
    void Phys_DrawVerticalRotatedString(uint8_t x, uint8_t y, const char *str, uint8_t color);

    // 显示状态（静态存储期对象 g_oled 由启动代码零初始化；无默认成员初始化以保持 constinit 常量初始化）
    uint8_t logic_buffer[kLogicBufferSize]; // 32x128 逻辑图形缓冲区
    uint8_t phys_buffer[kPhysBufferSize];   // 128x32 物理显示缓冲区
    uint8_t dot_exist;                      // 圆点显示标志（由 set_dot 设置）
};

// 写命令字节（控制字节 0x00 + 命令），超时 100 ms
void OledDriver::I2C_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 100);
}

// 写数据字节（控制字节 0x40 + 数据），超时 100 ms
void OledDriver::I2C_WriteData(uint8_t data)
{
    uint8_t buf[2] = {0x40, data};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 100);
}

// 画一个像素：color 非 0 置位，否则清零
void OledDriver::Logic_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
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

// 填充矩形（含边界）
void OledDriver::Logic_FillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    for (uint8_t y = y1; y <= y2; y++)
        for (uint8_t x = x1; x <= x2; x++)
            Logic_DrawPixel(x, y, color);
}

// 画矩形边框（含边界）
void OledDriver::Logic_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
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

// 电池图标（宽 24、高 12，level 为点亮段数 0~5）
void OledDriver::Logic_DrawBattery(uint8_t x, uint8_t y, uint8_t level)
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

// 按 show 决定是否绘制蓝牙图标
void OledDriver::Logic_DrawBluetooth(uint8_t x, uint8_t y, uint8_t show)
{
    if (!show)
        return;
    for (uint8_t i = 0; i < 20; i++)
        for (uint8_t j = 0; j < 16; j++)
            if (bluetooth_16x20[i][j])
                Logic_DrawPixel(x + j, y + i, 1);
}

// 绘制实心圆点（半径 2 像素，exist 非 0 时绘制）
void OledDriver::Logic_DrawDot(uint8_t x, uint8_t y, uint8_t exist)
{
    if (!exist)
        return;
    for (int8_t dy = -2; dy <= 2; dy++)
        for (int8_t dx = -2; dx <= 2; dx++)
            if (dx * dx + dy * dy <= 4)
                Logic_DrawPixel(x + dx, y + dy, 1);
}

// 进度条（在指定区域内从下向上填充）
void OledDriver::Logic_DrawProgressBar(uint8_t percent, uint8_t start_y, uint8_t area_height)
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

// 软件旋转：逻辑 32x128 顺时针旋转 90° -> 物理 128x32
void OledDriver::RotateLogicToPhysical(void)
{
    std::memset(phys_buffer, 0, sizeof(phys_buffer));
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

// 绘制侧躺字符（顺时针旋转 90°，并按 TEXT_MIRROR_H/V 做镜像调整）
void OledDriver::Phys_DrawCharRotated(uint8_t x, uint8_t y, char ch, uint8_t color)
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

// 竖直绘制侧躺字符串（TEXT_MODE==0 时使用；每个字符侧躺，从上到下排列，可反转顺序）
void OledDriver::Phys_DrawVerticalRotatedString(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    // 若启用 TEXT_REVERSE_ORDER，则先复制并反转字符串
    char reversed[5];
    const char *p_str = str;
#if TEXT_REVERSE_ORDER
    uint8_t len = 0;
    while (p_str[len])
        len++;
    for (uint8_t i = 0; i < len; i++)
    {
        reversed[i] = p_str[len - 1 - i];
    }
    reversed[len] = 0;
    p_str = reversed;
#endif

    uint8_t y_offset = 0;
    while (*p_str)
    {
        Phys_DrawCharRotated(x, y + y_offset, *p_str++, color);
        y_offset += TEXT_CHAR_SPACING;
        if (y_offset + 8 > OLED_PHYS_HEIGHT)
            break;
    }
}

/*
 * 功能：刷新一帧画面
 * 参数：battery_level - 电量等级（0~5）；bluetooth_state - 蓝牙状态；percent - 进度百分比
 * 说明：绘制电池、蓝牙、圆点与进度条，并叠加百分比文字后输出。
 */
void OledDriver::update(uint8_t battery_level, uint8_t bluetooth_state, uint8_t percent)
{
    std::memset(logic_buffer, 0, sizeof(logic_buffer));

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
    Phys_DrawHorizontalString(phys_buffer, TEXT_POS_X, TEXT_POS_Y, text, 1);
#endif

    // 复用对外 C 接口（与原实现一致），同时保证 OLED_Refresh 符号不被 --gc-sections 回收
    OLED_Refresh();
}

/* 清空逻辑与物理缓冲区 */
void OledDriver::clear(void)
{
    std::memset(logic_buffer, 0, sizeof(logic_buffer));
    std::memset(phys_buffer, 0, sizeof(phys_buffer));
}

/* 将物理缓冲区按页写入 OLED */
void OledDriver::refresh(void)
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

/* 初始化 OLED：上电延时后发送 SSD1306 初始化命令序列 */
void OledDriver::init(void)
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

    // 复用对外 C 接口（与原实现一致），同时保证 OLED_Clear/OLED_Refresh 符号不被 --gc-sections 回收
    OLED_Clear();
    OLED_Refresh();
}

// 文件内唯一实例：静态存储期实例。
OledDriver g_oled;

} // namespace

/* ==================== extern "C" 外观层（保持原有 ABI） ==================== */

/* 刷新一帧画面 */
void OLED_Update(uint8_t battery_level, uint8_t bluetooth_state, uint8_t percent)
{
    g_oled.update(battery_level, bluetooth_state, percent);
}

/* 设置右上角圆点是否显示 */
void OLED_Set_Dot(uint8_t exist)
{
    g_oled.set_dot(exist);
}

/* 清空逻辑与物理缓冲区 */
void OLED_Clear(void)
{
    g_oled.clear();
}

/* 将物理缓冲区按页写入 OLED */
void OLED_Refresh(void)
{
    g_oled.refresh();
}

/* 初始化 OLED */
void OLED_Init(void)
{
    g_oled.init();
}
