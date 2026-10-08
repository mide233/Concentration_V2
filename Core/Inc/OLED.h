#ifndef OLED_H
#define OLED_H

#include "main.h"
#include <stdint.h>

// 硬件分辨率
#define OLED_PHYS_WIDTH  128
#define OLED_PHYS_HEIGHT 32

// 图形逻辑分辨率（旋转后视觉竖屏）
#define OLED_LOGIC_WIDTH  32
#define OLED_LOGIC_HEIGHT 128

// 三个区域高度（图形逻辑坐标系，像素）
#define TOP_AREA_HEIGHT    15
#define MID_AREA_HEIGHT    15
#define BOTTOM_AREA_HEIGHT 98

// ========== 文字调试参数 ==========
// 文字模式：0 = 侧躺 + 竖直排列（垂直于进度条），1 = 正立 + 水平排列
#define TEXT_MODE 0

// 文字起始坐标（物理屏幕坐标，X:0~127, Y:0~31）
#define TEXT_POS_X 110
#define TEXT_POS_Y 4

// 字符间距（像素）：模式0（竖直排列）时表示上下两个字符中心间距；模式1（水平排列）时表示左右间距
#define TEXT_CHAR_SPACING 10

// 文字镜像修正（仅对模式0侧躺文字有效）
#define TEXT_MIRROR_H 0 // 水平镜像（左右翻转）
#define TEXT_MIRROR_V 1 // 垂直镜像（上下翻转）

// 文字顺序修正（仅对模式0竖直排列有效）：1 = 反转字符串顺序（从上到下正序），0 = 保持原顺序（可能从下到上）
#define TEXT_REVERSE_ORDER 1
// =================================

extern I2C_HandleTypeDef hi2c1;
#define OLED_ADDR 0x78

void OLED_Init(void);
void OLED_Update(uint8_t battery_level, uint8_t bluetooth_state, uint8_t percent);
void OLED_Set_Dot(uint8_t exist);
void OLED_Clear(void);
void OLED_Refresh(void);

#endif