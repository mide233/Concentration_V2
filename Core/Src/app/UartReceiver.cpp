/*
 * USART1 接收实现：DMA 循环搬运 + IDLE 中断判定一帧结束。
 * 行为与历史 stm32f1xx_it.c 中的内联实现完全一致。
 */
#include "app/UartReceiver.hpp"

#include "usart.h"

#include <cstring>

namespace app {

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
        /* 1. 清除空闲中断标志位 */
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);

        /* 2. 停止 DMA 接收，防止数据被后续覆盖 */
        HAL_UART_DMAStop(&huart1);

        /* 3. 计算本次接收到的数据长度 */
        frameLen_ = static_cast<uint16_t>(RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));

        /* 4. 将数据从 DMA 缓冲区拷贝到工作缓冲区，以便安全处理 */
        std::memcpy(workBuffer_, dmaBuffer_, frameLen_);

        /* 5. 设置标志，通知有新的数据帧需要处理（当前无消费者） */
        frameReady_ = 1;

        /* 6. 重新启动 DMA 接收，准备接收下一帧 */
        HAL_UART_Receive_DMA(&huart1, dmaBuffer_, RX_BUFFER_SIZE);
    }
}

} // namespace app

/* C 接缝：供 stm32f1xx_it.c（C）调用 */
extern "C" void UartReceiver_HandleIdle(void)
{
    app::g_uartReceiver.handleIdleInterrupt();
}
