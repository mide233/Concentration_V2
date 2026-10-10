#ifndef APP_MEASUREMENT_HPP
#define APP_MEASUREMENT_HPP

#include <cstdint>

namespace app {

struct ConversionValue {
    float rawValue;
    uint16_t uvLightLevel = 1;
};

/* 一次检测的状态标志（数值沿用历史约定）。 */
enum class ConversionFlag : uint8_t {
    Ready = 1,
    Finished = 2,
    InProgress = 3,
};

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
    void init(TimeUnit timeUnit, float onceDetectionTime) {
        // [R2 已修复] 原历史代码存在未初始化局部量 c_Unit，且其结果只写入从不读取的
        //             unit_ 字段。此处直接删除该无用字段与赋值，消除未定义行为。
        onceDetectionTime_ = onceDetectionTime;

        if (timeUnit == TimeUnit::Seconds) {
            timeUnit_ = 1.0f;
        }
        if (timeUnit == TimeUnit::Milliseconds) {
            timeUnit_ = 0.001f;
        }
        if (timeUnit == TimeUnit::Microseconds) {
            timeUnit_ = 0.000001f;
        }
    }

    void update(float current, float deltaTime) {
        if (flag_ == ConversionFlag::Ready) {
            flag_ = ConversionFlag::InProgress;
            currentStart_ = current;
        } else if (flag_ == ConversionFlag::InProgress) {
            currentTotal_ += (current - currentStart_) * deltaTime;
            detectionTime_ += deltaTime * timeUnit_;
            if (detectionTime_ >= onceDetectionTime_) {
                flag_ = ConversionFlag::Finished;
            }
        }
    }

    float result() {
        float out = 0.0f;
        if (flag_ == ConversionFlag::Finished) {
            out = currentTotal_ - value_.rawValue;
            currentTotal_ = 0.0f;
            detectionTime_ = 0.0f;
            flag_ = ConversionFlag::Ready;
        }
        return out;
    }

    bool calibrate(float current, float deltaTime) {
        update(current, deltaTime);
        if (flag_ == ConversionFlag::Finished) {
            value_.rawValue = currentTotal_;
            currentTotal_ = 0.0f;
        }
        return flag_ == ConversionFlag::Finished;
    }

    void reset() {
        flag_ = ConversionFlag::Ready;
        currentTotal_ = 0.0f;
        detectionTime_ = 0.0f;
    }

    [[nodiscard]] ConversionFlag flag() const { return flag_; }
    [[nodiscard]] const ConversionValue& value() const { return value_; }
    [[nodiscard]] float detectionTime() const { return detectionTime_; }

private:
    float timeUnit_ = 0.0f;
    float detectionTime_ = 0.0f;
    float onceDetectionTime_ = 0.0f;
    float currentTotal_ = 0.0f;
    float currentStart_ = 0.0f;
    ConversionValue value_{};
    ConversionFlag flag_ = ConversionFlag::Ready;
};

} // namespace app

#endif // APP_MEASUREMENT_HPP
