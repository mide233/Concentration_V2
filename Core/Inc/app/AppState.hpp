#ifndef APP_APPSTATE_HPP
#define APP_APPSTATE_HPP

#include <cstdint>

#include "app/Measurement.hpp"

namespace app {

/* 电池/充电状态（数值沿用历史约定：0=正常, 1=待机, 2=充电）。 */
enum class BattStatus : uint8_t {
    Normal = 0,
    Standby = 1,
    Charging = 2,
};

/* 工作状态机状态（数值沿用历史约定，顺序不可变）。 */
enum class WorkState : uint8_t {
    Init = 0,
    Save = 1,
    Calibration = 2,
    Working = 3,
    Ready = 4,
    ErrTilt = 5,
    ErrOpen = 6,
    ErrLowPower = 7,
    ErrNoContainer = 8,
};

/* 电池采样数据。 */
struct BatteryData {
    float adc;
    int level;
    BattStatus status;
};

/* 按键/开关消抖状态（原 hardware_status）。 */
struct InputState {
    uint8_t swStatus;
    uint8_t swTimes;
    uint8_t tiltStatus;
    uint8_t tiltTimes;
    uint8_t keyStatus;
    uint8_t keyTimes;
};

/* 应用运行数据（原全局 my_data / hardware_status）。 */
struct AppData {
    uint16_t adcValue[20];
    uint16_t adcAvg[2];
    BatteryData battery;
    float adcInt;
    WorkState workStatus;
    WorkState hopeStatus;
    float result;
    uint8_t progress;
    InputState input;
    Measurement measurement;
    uint64_t time;
};

} // namespace app

#endif // APP_APPSTATE_HPP
