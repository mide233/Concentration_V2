#ifndef APP_MEASUREMENT_HPP
#define APP_MEASUREMENT_HPP

#include <cstdint>

#include "app/Persistence.hpp"

namespace app {

/* 时间单位（枚举值沿用历史约定：ms=1, s=2, us=3）。 */
enum class TimeUnit : uint8_t {
    Milliseconds = 1,
    Seconds = 2,
    Microseconds = 3,
};

/*
 * 浓度/转换测量状态机：纯计算，不直接访问硬件。
 * 校准值通过 PersistentStore 读写 Flash。
 */
class Measurement {
public:
    void init(TimeUnit timeUnit, float onceDetectionTime);
    void update(float current, float deltaTime);
    bool calibrate(float current, float deltaTime);
    float result();
    void reset();

    [[nodiscard]] ConversionFlag flag() const { return flag_; }
    [[nodiscard]] const ConversionValue &value() const { return value_; }
    [[nodiscard]] float detectionTime() const { return detectionTime_; }

private:
    float unit_ = 0.0f; // [R2] 历史字段：写入未初始化值且从不读取，按决策保留原行为
    float timeUnit_ = 0.0f;
    float detectionTime_ = 0.0f;
    float onceDetectionTime_ = 0.0f;
    float currentTotal_ = 0.0f;
    float currentStart_ = 0.0f;
    ConversionValue value_{};
    ConversionFlag flag_ = ConversionFlag::NotInitialized;
};

} // namespace app

#endif // APP_MEASUREMENT_HPP
