#include "app/Measurement.hpp"

namespace app {

void Measurement::init(TimeUnit timeUnit, float onceDetectionTime)
{
    // [R2] 历史遗留：c_Unit 未初始化即参与运算，结果写入从不读取的 unit_。
    //      按“仅修 R3/R4”的决策保留原行为。
    float c_Unit;
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

    flag_ = PersistentStore::load(value_) ? ConversionFlag::Ready : ConversionFlag::NotInitialized;

    unit_ = timeUnit_ * c_Unit;
}

void Measurement::update(float current, float deltaTime)
{
    if (flag_ == ConversionFlag::Ready || flag_ == ConversionFlag::NotInitialized) {
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

float Measurement::result()
{
    float out = 0.0f;
    if (flag_ == ConversionFlag::Finished) {
        out = currentTotal_ - value_.rawValue;
        currentTotal_ = 0.0f;
        detectionTime_ = 0.0f;
        flag_ = ConversionFlag::Ready;
    }
    return out;
}

bool Measurement::calibrate(float current, float deltaTime)
{
    update(current, deltaTime);
    if (flag_ == ConversionFlag::Finished) {
        value_.rawValue = currentTotal_;
        currentTotal_ = 0.0f;
    }
    return flag_ == ConversionFlag::Finished;
}

void Measurement::reset()
{
    if (flag_ != ConversionFlag::NotInitialized) {
        flag_ = ConversionFlag::Ready;
    }
    currentTotal_ = 0.0f;
    detectionTime_ = 0.0f;
}

} // namespace app
