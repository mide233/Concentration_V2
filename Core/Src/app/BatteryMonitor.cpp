#include "app/BatteryMonitor.hpp"

#include "main.h"

#include "app/AppConfig.hpp"

namespace app {

void BatteryMonitor::setAdc(float adc)
{
    adc_ = adc;
}

/* 读取 SHDBY/CHRG 引脚判定充电状态（顺序沿用历史实现）。 */
void BatteryMonitor::updateStatus()
{
    if (HAL_GPIO_ReadPin(SHDBY_GPIO_Port, SHDBY_Pin) == GPIO_PIN_RESET) {
        status_ = BattStatus::Standby;
    } else if (HAL_GPIO_ReadPin(CHRG_GPIO_Port, CHRG_Pin) == GPIO_PIN_RESET) {
        status_ = BattStatus::Charging;
    } else {
        status_ = BattStatus::Normal;
    }
}

int BatteryMonitor::levelFor(float voltage)
{
    if (voltage >= kBatteryVMax) {
        return 5;
    }
    if (voltage <= kBatteryVMin) {
        return 0;
    }

    const float ratio = (voltage - kBatteryVMin) / (kBatteryVMax - kBatteryVMin);
    int level = static_cast<int>(ratio * 5 + 0.5f);

    if (level < 0) {
        level = 0;
    }
    if (level > 5) {
        level = 5;
    }
    return level;
}

} // namespace app
