#ifndef APP_APPSTATE_HPP
#define APP_APPSTATE_HPP

#include <cstdint>

#include "app/BatteryMonitor.hpp"
#include "app/Hardware.hpp"
#include "app/Measurement.hpp"

namespace app {

/* 工作状态机状态（数值沿用历史约定，顺序不可变）。 */
enum class WorkState : uint8_t {
    Calibration = 2,
    Working = 3,
    Ready = 4,
    ErrTilt = 5,
    ErrOpen = 6,
    ErrLowPower = 7,
    ErrNoContainer = 8,
};

/* 三路按键/开关输入（原 hardware_status 的 SW/TILT/KEY）。 */
struct Inputs {
    InputDebounce sw;
    InputDebounce tilt;
    InputDebounce key;
};

/* 应用运行数据（原全局 my_data / hardware_status）。 */
struct AppData {
    uint16_t adcValue[20];
    uint16_t adcAvg[2];
    BatteryMonitor battery;
    float adcInt;
    WorkState workStatus;
    WorkState hopeStatus;
    float result;
    uint8_t progress;
    Inputs input;
    Measurement measurement;
    uint64_t time;
};

} // namespace app

#endif // APP_APPSTATE_HPP
