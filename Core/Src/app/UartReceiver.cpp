/*
 * USART1 接收实现：DMA 循环搬运 + IDLE 中断判定一帧结束。
 * R8：中断仅置挂起位，耗时的帧长度计算与拷贝移至主循环 poll()。
 */
#include "app/UartReceiver.hpp"

#include "usart.h"

#include <cstring>

namespace app
{

    UartReceiver g_uartReceiver;

    void UartReceiver::start()
    {
        HAL_UART_Receive_DMA(&huart1, dmaBuffer_, RX_BUFFER_SIZE);
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    }

    void UartReceiver::handleIdleInterrupt()
    {
        /* 判断是否为 USART1 的空闲中断 */
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
            /* 仅清空闲中断标志并置挂起位；不在中断内做 DMAStop/memcpy/重启。 */
            __HAL_UART_CLEAR_IDLEFLAG(&huart1);
            framePending_ = true;
        }
    }

    void UartReceiver::poll()
    {
        if (!framePending_) {
            return;
        }
        framePending_ = false;

        /* RX DMA 为循环模式且持续运行：用当前写位置与上次处理位置的增量求帧长。
         * 原实现在中断内 DMAStop 后以“绝对写位置”为帧长、从缓冲区起始拷贝，并重启
         * DMA 将位置清零；此处以增量位置（含回绕）在语义上等价地还原该帧。 */
        const uint16_t pos = static_cast<uint16_t>(RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));

        if (pos >= prevPos_) {
            frameLen_ = static_cast<uint16_t>(pos - prevPos_);
            if (frameLen_ > 0u) {
                std::memcpy(workBuffer_, dmaBuffer_ + prevPos_, frameLen_);
            }
        } else {
            const uint16_t tail = static_cast<uint16_t>(RX_BUFFER_SIZE - prevPos_);
            std::memcpy(workBuffer_, dmaBuffer_ + prevPos_, tail);
            std::memcpy(workBuffer_ + tail, dmaBuffer_, pos);
            frameLen_ = static_cast<uint16_t>(tail + pos);
        }
        prevPos_ = pos;

        /* 设置标志，通知有新的数据帧需要处理（当前无消费者） */
        frameReady_ = 1;
    }

} // namespace app

/* C 接缝：供 stm32f1xx_it.c（C）调用 */
extern "C" void UartReceiver_HandleIdle(void)
{
    app::g_uartReceiver.handleIdleInterrupt();
}
