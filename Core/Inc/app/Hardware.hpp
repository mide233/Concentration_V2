#ifndef APP_HARDWARE_HPP
#define APP_HARDWARE_HPP

#include <cstdint>
#include "tim.h"

namespace app
{

    /* UV 灯亮度：TIM2_CH4 比较值 = 4 × level */
    inline void setUvLevel(std::uint16_t level)
    {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 4 * level);
    }

    inline void dcCtrlOn()
    {
        HAL_GPIO_WritePin(DC_ctrl_GPIO_Port, DC_ctrl_Pin, GPIO_PIN_RESET);
    }

    inline void dcCtrlOff()
    {
        HAL_GPIO_WritePin(DC_ctrl_GPIO_Port, DC_ctrl_Pin, GPIO_PIN_SET);
    }

} // namespace app

#endif /* APP_HARDWARE_HPP */
