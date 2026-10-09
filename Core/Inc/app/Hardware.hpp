#ifndef APP_HARDWARE_HPP
#define APP_HARDWARE_HPP

#include <cstdint>

#include "main.h"

/*
 * 板级硬件操作的内联封装。
 * 取代 main.h 中直接展开到寄存器/HAL 的宏，避免宏副作用并便于静态检查。
 */

namespace app {

/* UV 灯亮度：TIM2_CH4 比较值 = 4 × level（沿用原宏的换算） */
inline void setUvLevel(std::uint16_t level) {
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 4 * level);
}

/* 负载电源开关：原宏 ON=RESET、OFF=SET */
inline void dcCtrlOn() {
    HAL_GPIO_WritePin(DC_ctrl_GPIO_Port, DC_ctrl_Pin, GPIO_PIN_RESET);
}

inline void dcCtrlOff() {
    HAL_GPIO_WritePin(DC_ctrl_GPIO_Port, DC_ctrl_Pin, GPIO_PIN_SET);
}

} // namespace app

#endif /* APP_HARDWARE_HPP */
