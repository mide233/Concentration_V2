#ifndef APP_UARTRECEIVER_HPP
#define APP_UARTRECEIVER_HPP

#include <cstdint>

#include <cstring>

#include "main.h"
#include "usart.h"

namespace app {

/*
 * USART1 接收：DMA 循环搬运 + IDLE 中断判定一帧结束。
 * R8：中断内不再做 DMAStop/memcpy/重启等阻塞操作，仅清 IDLE 标志并置挂起位；
 *     帧长度计算与拷贝在 poll()（主循环上下文）完成。RX DMA 保持循环运行。
 * 说明：收帧结果由 takeFrame() 在主循环消费（蓝牙模块，见 Bluetooth.hpp），
 *       风险 R6（收帧无消费者）由此消除。
 */
class UartReceiver {
public:
    void start()               // 启动 DMA 接收并使能 IDLE 中断
    {
        /* 清除上电期间可能已锁存的溢出/错误标志：模块在 DMA 装配前即可能已发送数据，
         * 令 USART1 SR 的 ORE 置位；若不清除，HAL_UART_IRQHandler 会在错误中断中调用
         * UART_EndRxTransfer 中止循环 DMA 且不再重启（接收永久失效）。*/
        __HAL_UART_CLEAR_OREFLAG(&huart1);

        prevPos_ = 0;
        HAL_UART_Receive_DMA(&huart1, dmaBuffer_, RX_BUFFER_SIZE);
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    }

    // 接收错误恢复（由 HAL_UART_ErrorCallback 在中断上下文调用）：
    // 仅清除错误标志并置重启请求，实际重启延迟到主循环 poll() 执行，避免在 ISR 内
    // 重入 HAL 的 DMA 装配流程。
    void onError()
    {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        needRestart_ = true;
    }

    void handleIdleInterrupt() // USART1 IDLE 中断处理（仅清标志 + 置挂起位）
    {
        /* 判断是否为 USART1 的空闲中断 */
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
            /* 仅清空闲中断标志并置挂起位；不在中断内做 DMAStop/memcpy/重启。 */
            __HAL_UART_CLEAR_IDLEFLAG(&huart1);
            framePending_ = true;
        }
    }

    void poll() // 主循环：完成帧长度计算与数据拷贝
    {
        /* 接收链路自愈：错误中断可能中止了循环 DMA，在此重新装配。 */
        if (needRestart_) {
            needRestart_ = false;
            start();
        }

        if (!framePending_) {
            return;
        }
        framePending_ = false;

        /* RX DMA 为循环模式且持续运行：用当前写位置与上次处理位置的增量求帧长。
         * 原实现在中断内 DMAStop 后以“绝对写位置”为帧长、从缓冲区起始拷贝，并重启
         * DMA 将位置清零；此处以增量位置（含回绕）在语义上等价地还原该帧。 */
        const uint16_t pos =
            static_cast<uint16_t>(RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));

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

        /* 设置标志，通知有新的数据帧可供 takeFrame() 消费 */
        frameReady_ = 1;
    }

    // 消费当前已就绪的一帧：拷贝至 out 并清除标志；返回拷贝字节数（无帧返回 0）。
    uint16_t takeFrame(uint8_t* out, uint16_t cap)
    {
        if (!frameReady_) {
            return 0;
        }
        frameReady_ = 0;

        uint16_t len = frameLen_;
        if (len > cap) {
            len = cap;
        }
        std::memcpy(out, workBuffer_, len);
        return len;
    }

private:
    uint8_t dmaBuffer_[RX_BUFFER_SIZE];  // DMA 搬运缓冲区
    volatile bool framePending_;         // ISR 置位 / poll() 清零
    volatile uint8_t frameReady_;        // 帧接收完成标志
    uint8_t workBuffer_[RX_BUFFER_SIZE]; // 帧处理工作缓冲区
    uint16_t frameLen_;                  // 当前帧长度
    uint16_t prevPos_;                   // 上次处理时的 DMA 写位置
    volatile bool needRestart_;          // 错误中断请求重启接收 DMA
};

inline UartReceiver g_uartReceiver;

} // namespace app

#endif
