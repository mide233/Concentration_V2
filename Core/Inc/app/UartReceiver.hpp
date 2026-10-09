#ifndef APP_UARTRECEIVER_HPP
#define APP_UARTRECEIVER_HPP

#include <cstdint>

#include "main.h"

namespace app {

/*
 * USART1 接收：DMA 循环搬运 + IDLE 中断判定一帧结束。
 * 说明：收帧结果当前无消费者（见 docs/DEAD_CODE.md，风险 R8），本类仅保留
 *       原有运行行为，不新增消费逻辑。
 */
class UartReceiver {
public:
    void start();               // 启动 DMA 接收并使能 IDLE 中断
    void handleIdleInterrupt(); // USART1 IDLE 中断处理（帧结束）

private:
    uint8_t dmaBuffer_[RX_BUFFER_SIZE];  // DMA 搬运缓冲区
    volatile uint8_t frameReady_;        // 帧接收完成标志
    uint8_t workBuffer_[RX_BUFFER_SIZE]; // 帧处理工作缓冲区
    uint16_t frameLen_;                  // 当前帧长度
};

extern UartReceiver g_uartReceiver;

} // namespace app

#endif
