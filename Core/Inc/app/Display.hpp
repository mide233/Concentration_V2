#ifndef APP_DISPLAY_HPP
#define APP_DISPLAY_HPP

#include <cstdint>
#include <cstring>

#include "app/OledPanel.hpp"

namespace app {

// 文字（侧躺、竖直排列）参数
inline constexpr uint8_t kTextPosX = 110;
inline constexpr uint8_t kTextPosY = 2;
inline constexpr uint8_t kTextCharSpacing = 10;

/*
 * 用户界面组合。
 * 在逻辑坐标系（32x128，旋转后视觉为竖屏）绘制电池、蓝牙、圆点与进度条，
 * 并叠加百分比文字；随后软件旋转到物理缓冲并经 OledPanel 输出。
 */
class Display {
public:
    static constexpr uint8_t kLogicWidth = 32;
    static constexpr uint8_t kLogicHeight = 128;
    static constexpr uint8_t kTopAreaHeight = 15;
    static constexpr uint8_t kMidAreaHeight = 15;
    static constexpr uint8_t kBottomAreaHeight = 98;

    // 初始化面板并清空逻辑缓冲
    void init() {
        std::memset(logicBuffer_, 0, sizeof(logicBuffer_));
        panel_.init();
    }

    // 绘制并刷新一帧
    void update(
        uint8_t batteryLevel, uint8_t bluetoothState, uint8_t percent, const char* msg = nullptr,
        bool is_err = false) {
        std::memset(logicBuffer_, 0, sizeof(logicBuffer_));

        uint8_t topCenterY = static_cast<uint8_t>(kTopStartY + kTopAreaHeight / 2);
        drawBattery(4, static_cast<uint8_t>(topCenterY - 6), batteryLevel);

        uint8_t midCenterY = static_cast<uint8_t>(kMidStartY + kMidAreaHeight / 2);
        drawBluetooth(5, static_cast<uint8_t>(midCenterY - 9));
        drawDot(static_cast<uint8_t>(kLogicWidth - 11), midCenterY - 1, bluetoothState);

        drawProgressBar(percent, kBottomStartY, kBottomAreaHeight);

        rotateToPanel();
        drawText(percent, is_err);

        if (msg) {
            drawString(38, 9, msg, true);
        }

        panel_.refresh();
    }

private:
    static_assert(
        kTopAreaHeight + kMidAreaHeight + kBottomAreaHeight == kLogicHeight,
        "logical area heights must sum to logical height");
    static constexpr uint16_t kLogicBufferSize = kLogicWidth * (kLogicHeight / 8); // 512
    static_assert(kLogicBufferSize == 512, "logic buffer must be 512 bytes");

    // 逻辑坐标系下的区域起始 Y
    static constexpr uint8_t kTopStartY = 0;
    static constexpr uint8_t kMidStartY = kTopAreaHeight;
    static constexpr uint8_t kBottomStartY = kTopAreaHeight + kMidAreaHeight;

    // 逻辑坐标置位（超界忽略）
    void drawPixel(uint8_t x, uint8_t y) {
        if (x >= kLogicWidth || y >= kLogicHeight)
            return;
        logicBuffer_[(y / 8) * kLogicWidth + x] |= static_cast<uint8_t>(1u << (y % 8));
    }

    // 填充矩形（含边界）
    void fillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
        for (uint8_t y = y1; y <= y2; y++)
            for (uint8_t x = x1; x <= x2; x++)
                drawPixel(x, y);
    }

    // 画矩形边框（含边界）
    void drawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
        for (uint8_t x = x1; x <= x2; x++) {
            drawPixel(x, y1);
            drawPixel(x, y2);
        }
        for (uint8_t y = y1 + 1; y < y2; y++) {
            drawPixel(x1, y);
            drawPixel(x2, y);
        }
    }

    // 电池图标（宽 24、高 12，level 为点亮段数 0~5）
    void drawBattery(uint8_t x, uint8_t y, uint8_t level) {
        drawRect(x, y, static_cast<uint8_t>(x + 22), static_cast<uint8_t>(y + 11));
        fillRect(
            static_cast<uint8_t>(x + 23), static_cast<uint8_t>(y + 3), static_cast<uint8_t>(x + 24),
            static_cast<uint8_t>(y + 8));
        constexpr uint8_t kSegWidth = 4;
        for (uint8_t i = 0; i < level; i++) {
            uint8_t ix = static_cast<uint8_t>(x + 2 + i * kSegWidth);
            if (ix + 2 <= x + 20)
                fillRect(
                    ix, static_cast<uint8_t>(y + 2), static_cast<uint8_t>(ix + 2),
                    static_cast<uint8_t>(y + 9));
        }
    }

    void drawBluetooth(uint8_t x, uint8_t y) {
        for (uint8_t i = 0; i < 20; i++)
            for (uint8_t j = 0; j < 16; j++)
                if (kBluetooth16x20[i][j])
                    drawPixel(static_cast<uint8_t>(x + j), static_cast<uint8_t>(y + i));
    }

    // 实心圆点（半径 2 像素，exist 非 0 时绘制）
    void drawDot(uint8_t x, uint8_t y, uint8_t exist) {
        for (int8_t dy = -2; dy <= 2; dy++)
            for (int8_t dx = -2; dx <= 2; dx++) {
                int8_t distSq = dx * dx + dy * dy;
                if (exist ? (distSq <= 4) : (distSq == 4))
                    drawPixel(static_cast<uint8_t>(x + dx), static_cast<uint8_t>(y + dy));
            }
    }

    // 进度条（在指定区域内从下向上填充）
    void drawProgressBar(uint8_t percent, uint8_t startY, uint8_t areaHeight) {
        uint8_t barHeight = static_cast<uint8_t>(areaHeight - 20);
        if (barHeight < 8)
            barHeight = 8;
        constexpr uint8_t kBarWidth = 28;
        uint8_t startX = static_cast<uint8_t>((kLogicWidth - kBarWidth) / 2);
        drawRect(
            startX, startY, static_cast<uint8_t>(startX + kBarWidth - 1),
            static_cast<uint8_t>(startY + barHeight - 1));

        uint8_t innerX1 = static_cast<uint8_t>(startX + 5);
        uint8_t innerX2 = static_cast<uint8_t>(startX + kBarWidth - 1 - 5);
        uint8_t innerY1 = static_cast<uint8_t>(startY + 5);
        uint8_t innerY2 = static_cast<uint8_t>(startY + barHeight - 1 - 5);
        if (innerX1 > innerX2 || innerY1 > innerY2)
            return;

        uint8_t span = static_cast<uint8_t>(innerY2 - innerY1 + 1);
        uint8_t fillHeight = static_cast<uint8_t>((percent * span + 50) / 100);
        if (fillHeight > span)
            fillHeight = span;
        if (fillHeight > 0) {
            uint8_t fillStartY = static_cast<uint8_t>(innerY2 - fillHeight + 1);
            fillRect(innerX1, fillStartY, innerX2, innerY2);
        }
    }

    // 软件旋转：逻辑 32x128 顺时针旋转 90° -> 物理 128x32
    void rotateToPanel() {
        panel_.clear();
        for (uint8_t ly = 0; ly < kLogicHeight; ly++) {
            for (uint8_t lx = 0; lx < kLogicWidth; lx++) {
                uint8_t color = (logicBuffer_[(ly / 8) * kLogicWidth + lx] >> (ly % 8)) & 0x01;
                if (color) {
                    uint8_t px = ly;
                    uint8_t py = static_cast<uint8_t>(kLogicWidth - 1 - lx);
                    panel_.setPixel(px, py);
                }
            }
        }
    }

    // 绘制侧躺字符（顺时针旋转 90°，垂直镜像）
    void drawCharRotated(uint8_t x, uint8_t y, char ch) {
        if (x + 16 > OledPanel::kWidth || y + 8 > OledPanel::kHeight)
            return;
        uint8_t idx = glyphIndex(ch);
        if (idx == 255)
            return;

        const uint8_t* glyph = &kFont8x16[static_cast<uint16_t>(idx) * 16u];
        for (uint8_t i = 0; i < 16; i++) {
            uint8_t line = glyph[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (line & (1u << j)) {
                    uint8_t nx = static_cast<uint8_t>(x + i);
                    uint8_t ny = static_cast<uint8_t>(y + j);
                    panel_.setPixel(nx, ny);
                }
            }
        }
    }

    // 竖直绘制侧躺字符串（字符从上到下排列，反转顺序以得到正序）
    void drawVerticalRotatedString(uint8_t x, uint8_t y, const char* str) {
        char reversed[5];
        const char* pStr = str;
        uint8_t len = 0;
        while (pStr[len])
            len++;
        for (uint8_t i = 0; i < len; i++)
            reversed[i] = pStr[len - 1 - i];
        reversed[len] = 0;
        pStr = reversed;

        uint8_t yOffset = 0;
        while (*pStr) {
            drawCharRotated(x, static_cast<uint8_t>(y + yOffset), *pStr++);
            yOffset = static_cast<uint8_t>(yOffset + kTextCharSpacing);
            if (yOffset + 8 > OledPanel::kHeight)
                break;
        }
    }

    void drawChar(uint8_t x, uint8_t y, char ch, bool reverse_color = false) {
        if (x + 8 > OledPanel::kWidth || y + 16 > OledPanel::kHeight)
            return;
        uint8_t idx = glyphIndex(ch);
        if (idx == 255)
            return;

        const uint8_t* glyph = &kFont8x16[static_cast<uint16_t>(idx) * 16u];
        for (uint8_t row = 0; row < 16; row++) {
            uint8_t line = glyph[row];
            for (uint8_t col = 0; col < 8; col++) {
                // 关键：bit 顺序反过来
                if (line & (1u << (7 - col))) {
                    panel_.setPixel(
                        static_cast<uint8_t>(x + col), static_cast<uint8_t>(y + row),
                        reverse_color);
                }
            }
        }
    }

    void drawString(uint8_t x, uint8_t y, const char* str, bool reverse_color = false) {
        uint8_t xOffset = 0;
        while (*str) {
            drawChar(static_cast<uint8_t>(x + xOffset), y, *str++, reverse_color);
            xOffset = static_cast<uint8_t>(xOffset + 8); // 字宽 8，无间距
            if (x + xOffset > OledPanel::kWidth)         // 超出右边界停止
                break;
        }
    }

    // 组装并绘制百分比文字（>=100 显示 OK!）
    void drawText(uint8_t percent, bool is_err) {
        char text[5] = {0};
        if (percent >= 100) {
            std::strcpy(text, "OK!");
        } else if (percent < 10) {
            text[0] = static_cast<char>('0' + percent);
            text[1] = '%';
        } else {
            text[0] = static_cast<char>('0' + (percent / 10));
            text[1] = static_cast<char>('0' + (percent % 10));
            text[2] = '%';
        }
        if (is_err) {
            std::strcpy(text, "ERR");
        }
        drawVerticalRotatedString(kTextPosX, kTextPosY, text);
    }

    OledPanel panel_;
    uint8_t logicBuffer_[kLogicBufferSize]; // 32x128 逻辑图形缓冲（按页存放）
};

} // namespace app

#endif
