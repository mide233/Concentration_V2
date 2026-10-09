/*
 * SSD1306（128x32，I2C1）底层驱动实现。
 * 负责命令/数据发送与物理缓冲刷新；图形绘制见 Display。
 */
#include "app/OledPanel.hpp"

#include "i2c.h"
#include <cstring>

namespace app
{
    namespace
    {

        constexpr uint8_t kOledAddress   = 0x78; // 7 位地址 0x3C 左移一位
        constexpr uint32_t kI2cTimeoutMs = 100;

        uint8_t txBuffer_[1 + OledPanel::kBufferSize];

    } // namespace

    // 写命令字节（控制字 0x00 + 命令）
    void OledPanel::writeCommand(uint8_t command)
    {
        uint8_t buf[2] = {0x00, command};
        HAL_I2C_Master_Transmit(&hi2c1, kOledAddress, buf, 2, kI2cTimeoutMs);
    }

    // 写数据字节（控制字 0x40 + 数据）
    void OledPanel::writeData(uint8_t data)
    {
        uint8_t buf[2] = {0x40, data};
        HAL_I2C_Master_Transmit(&hi2c1, kOledAddress, buf, 2, kI2cTimeoutMs);
    }

    // 物理坐标置位（超界忽略）
    void OledPanel::setPixel(uint8_t x, uint8_t y)
    {
        if (x >= kWidth || y >= kHeight)
            return;
        buffer_[(y / kPageBits) * kWidth + x] |= static_cast<uint8_t>(1u << (y % kPageBits));
    }

    // 清空物理缓冲
    void OledPanel::clear()
    {
        std::memset(buffer_, 0, sizeof(buffer_));
    }

    // 将物理缓冲整帧写入面板（方案 C：DMA 传输，主循环不再阻塞等待）。
    // 依赖 init() 已设置的“水平寻址模式”（0x20 0x00）：设置列 0..127、页 0..3 后，
    // 本次 513 字节（0x40 + 512）会连续覆盖全部 4 页。
    void OledPanel::refresh()
    {
        // 若上一帧 DMA 尚未结束，先等待，避免并发改写 txBuffer_（正常已空闲）。
        const uint32_t start = HAL_GetTick();
        while (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_BUSY_TX) {
            if (HAL_GetTick() - start >= kI2cTimeoutMs) {
                break; // 异常保护：交由 HAL 错误回调/超时处理
            }
        }

        // 设置水平寻址窗口：列 0..127，页 0..3（一条命令事务）
        uint8_t cmd[7] = {0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, 0x03};
        HAL_I2C_Master_Transmit(&hi2c1, kOledAddress, cmd, sizeof(cmd), kI2cTimeoutMs);

        // 控制字 0x40 + 512 字节显存，单次 DMA 发送后立即返回（约 11.5 ms @400 kHz）
        txBuffer_[0] = 0x40;
        std::memcpy(&txBuffer_[1], buffer_, kBufferSize);
        HAL_I2C_Master_Transmit_DMA(&hi2c1, kOledAddress, txBuffer_, sizeof(txBuffer_));
    }

    // 上电初始化序列
    void OledPanel::init()
    {
        HAL_Delay(100);
        writeCommand(0xAE); // display off
        writeCommand(0xD5);
        writeCommand(0x80);
        writeCommand(0xA8);
        writeCommand(0x1F); // multiplex 1/32
        writeCommand(0xD3);
        writeCommand(0x00);
        writeCommand(0x40);
        writeCommand(0x8D);
        writeCommand(0x14); // charge pump on
        writeCommand(0x20);
        writeCommand(0x00);
        writeCommand(0xA0);
        writeCommand(0xC0);
        writeCommand(0xDA);
        writeCommand(0x02);
        writeCommand(0x81);
        writeCommand(0x8F);
        writeCommand(0xD9);
        writeCommand(0xF1);
        writeCommand(0xDB);
        writeCommand(0x40);
        writeCommand(0xA4);
        writeCommand(0xA6);
        writeCommand(0x2E);
        writeCommand(0xAF); // display on

        clear();
        refresh();
    }

} // namespace app
