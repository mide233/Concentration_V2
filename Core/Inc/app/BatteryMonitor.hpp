#ifndef APP_BATTERYMONITOR_HPP
#define APP_BATTERYMONITOR_HPP

#include <cstdint>

#include "main.h"

#include "app/AppConfig.hpp"

namespace app {

/* 电池/充电状态（数值沿用历史约定：0=正常, 1=待机, 2=充电）。 */
enum class BattStatus : uint8_t {
    Normal = 0,
    Standby = 1,
    Charging = 2,
};

/*
 * 电池电压与充电状态。
 * levelFor() 为纯换算；level_ 沿用历史行为（从不由采样自动更新，恒为初值 0）。
 */
class BatteryMonitor {
public:
    void setAdc(float adc) { adc_ = adc; }

    /* 读取 STDBY/CHRG 引脚判定充电状态（顺序沿用历史实现）。 */
    void updateStatus() {
        if (HAL_GPIO_ReadPin(STDBY_GPIO_Port, STDBY_Pin) == GPIO_PIN_RESET) {
            status_ = BattStatus::Standby;
        } else if (HAL_GPIO_ReadPin(CHRG_GPIO_Port, CHRG_Pin) == GPIO_PIN_RESET) {
            status_ = BattStatus::Charging;
        } else {
            status_ = BattStatus::Normal;
        }
    }

    [[nodiscard]] float adc() const { return adc_; }
    [[nodiscard]] int level() const { return level_; }
    [[nodiscard]] BattStatus status() const { return status_; }

    [[nodiscard]] static int levelFor(float voltage) {
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

private:
    float adc_ = 0.0f;
    int level_ = 0;
    BattStatus status_ = BattStatus::Normal;
};

} // namespace app

#endif // APP_BATTERYMONITOR_HPP
