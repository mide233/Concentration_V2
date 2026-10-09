#ifndef APP_OLEDPANEL_HPP
#define APP_OLEDPANEL_HPP

#include <cstdint>

namespace app
{

    /*
     * SSD1306（128x32，I2C1）底层驱动。
     * 仅负责命令/数据发送、缓冲清零与按页刷新，以及物理像素的置位；
     * 不涉及图形布局与旋转（见 Display）。
     */
    class OledPanel
    {
    public:
        static constexpr uint8_t kWidth       = 128;
        static constexpr uint8_t kHeight      = 32;
        static constexpr uint8_t kPageBits    = 8;
        static constexpr uint16_t kBufferSize = kWidth * (kHeight / kPageBits); // 512

        void init();    // 上电延时 + SSD1306 初始化序列（含 clear + refresh）
        void clear();   // 清空物理缓冲
        void refresh(); // 将物理缓冲按页写入面板

        void setPixel(uint8_t x, uint8_t y); // 物理坐标置位

    private:
        static void writeCommand(uint8_t command);
        static void writeData(uint8_t data);

        uint8_t buffer_[kBufferSize]; // 128x32 物理缓冲（按页存放）
    };

} // namespace app

#endif
