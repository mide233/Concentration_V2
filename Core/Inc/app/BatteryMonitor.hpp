#ifndef APP_BATTERYMONITOR_HPP
#define APP_BATTERYMONITOR_HPP

#include <cstdint>

namespace app
{

    /* 电池/充电状态（数值沿用历史约定：0=正常, 1=待机, 2=充电）。 */
    enum class BattStatus : uint8_t {
        Normal   = 0,
        Standby  = 1,
        Charging = 2,
    };

    /*
     * 电池电压与充电状态。
     * levelFor() 为纯换算；level_ 沿用历史行为（从不由采样自动更新，恒为初值 0）。
     */
    class BatteryMonitor
    {
    public:
        void setAdc(float adc);
        void updateStatus();

        [[nodiscard]] float adc() const
        { return adc_; }
        [[nodiscard]] int level() const
        { return level_; }
        [[nodiscard]] BattStatus status() const
        { return status_; }

        [[nodiscard]] static int levelFor(float voltage);

    private:
        float adc_         = 0.0f;
        int level_         = 0;
        BattStatus status_ = BattStatus::Normal;
    };

} // namespace app

#endif // APP_BATTERYMONITOR_HPP
