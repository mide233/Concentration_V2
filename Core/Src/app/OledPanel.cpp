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

    // 将物理缓冲按页写入面板（方案 A：批量 I2C，显著减少事务数）
    void OledPanel::refresh()
    {
        for (uint8_t page = 0; page < kHeight / kPageBits; page++) {
            // 一条事务发送本页 3 个命令：控制字 0x00 之后连续字节均按命令解释
            uint8_t cmd[4] = {0x00, static_cast<uint8_t>(0xB0 + page), 0x00, 0x10};
            HAL_I2C_Master_Transmit(&hi2c1, kOledAddress, cmd, sizeof(cmd), kI2cTimeoutMs);

            // 一条事务发送本页 128 字节显存：控制字 0x40 + 数据
            uint8_t data[1 + kWidth];
            data[0] = 0x40;
            std::memcpy(&data[1], &buffer_[page * kWidth], kWidth);
            HAL_I2C_Master_Transmit(&hi2c1, kOledAddress, data, sizeof(data), kI2cTimeoutMs);
        }
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
