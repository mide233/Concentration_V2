#ifndef APP_UARTRECEIVER_HPP
#define APP_UARTRECEIVER_HPP

#include <cstdint>

#include "main.h"

namespace app {

/*
 * USART1 接收：DMA 循环搬运 + IDLE 中断判定一帧结束。
 * R8：中断内不再做 DMAStop/memcpy/重启等阻塞操作，仅清 IDLE 标志并置挂起位；
 *     帧长度计算与拷贝在 poll()（主循环上下文）完成。RX DMA 保持循环运行。
 * 说明：收帧结果当前无消费者（见 docs/DEAD_CODE.md，风险 R6），本类仅保留
 *       收帧能力，不新增消费逻辑。
 */
class UartReceiver {
public:
    void start();               // 启动 DMA 接收并使能 IDLE 中断
    void handleIdleInterrupt(); // USART1 IDLE 中断处理（仅清标志 + 置挂起位）
    void poll();                // 主循环：完成帧长度计算与数据拷贝

private:
    uint8_t dmaBuffer_[RX_BUFFER_SIZE]; // DMA 搬运缓冲区
    volatile bool framePending_;        // ISR 置位 / poll() 清零
    volatile uint8_t frameReady_;       // 帧接收完成标志
    uint8_t workBuffer_[RX_BUFFER_SIZE]; // 帧处理工作缓冲区
    uint16_t frameLen_;                 // 当前帧长度
    uint16_t prevPos_;                  // 上次处理时的 DMA 写位置
};

extern UartReceiver g_uartReceiver;

} // namespace app

#endif
