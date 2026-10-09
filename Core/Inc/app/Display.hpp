#ifndef APP_DISPLAY_HPP
#define APP_DISPLAY_HPP

#include <cstdint>

#include "app/OledPanel.hpp"

namespace app
{

    /*
     * 用户界面组合。
     * 在逻辑坐标系（32x128，旋转后视觉为竖屏）绘制电池、蓝牙、圆点与进度条，
     * 并叠加百分比文字；随后软件旋转到物理缓冲并经 OledPanel 输出。
     */
    class Display
    {
    public:
        static constexpr uint8_t kLogicWidth       = 32;
        static constexpr uint8_t kLogicHeight      = 128;
        static constexpr uint8_t kTopAreaHeight    = 15;
        static constexpr uint8_t kMidAreaHeight    = 15;
        static constexpr uint8_t kBottomAreaHeight = 98;

        void init();                                                                // 初始化面板并清空逻辑缓冲
        void setDot(bool on);                                                       // 设置中部圆点指示
        void update(uint8_t batteryLevel, uint8_t bluetoothState, uint8_t percent); // 绘制并刷新一帧

    private:
        static_assert(kTopAreaHeight + kMidAreaHeight + kBottomAreaHeight == kLogicHeight,
                      "logical area heights must sum to logical height");
        static constexpr uint16_t kLogicBufferSize = kLogicWidth * (kLogicHeight / 8); // 512
        static_assert(kLogicBufferSize == 512, "logic buffer must be 512 bytes");

        void drawPixel(uint8_t x, uint8_t y);
        void fillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
        void drawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
        void drawBattery(uint8_t x, uint8_t y, uint8_t level);
        void drawBluetooth(uint8_t x, uint8_t y, uint8_t show);
        void drawDot(uint8_t x, uint8_t y, uint8_t exist);
        void drawProgressBar(uint8_t percent, uint8_t startY, uint8_t areaHeight);
        void rotateToPanel();
        void drawCharRotated(uint8_t x, uint8_t y, char ch);
        void drawVerticalRotatedString(uint8_t x, uint8_t y, const char *str);
        void drawText(uint8_t percent);

        OledPanel panel_;
        uint8_t logicBuffer_[kLogicBufferSize]; // 32x128 逻辑图形缓冲（按页存放）
        uint8_t dotExist_;
    };

} // namespace app

#endif
