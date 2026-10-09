#ifndef APP_INPUTDEBOUNCE_HPP
#define APP_INPUTDEBOUNCE_HPP

#include <cstdint>

#include "app/AppConfig.hpp"

namespace app {

/*
 * 电平消抖：读入电平与当前状态连续不同超过 kDebounceThreshold 次才翻转。
 * 与历史实现一致：匹配时不重置计数，仅在翻转时归零。
 */
class InputDebounce {
public:
    void begin(uint8_t initialStatus)
    {
        status_ = initialStatus;
        times_ = 0;
    }

    void update(uint8_t pinLevel)
    {
        if (pinLevel != status_) {
            ++times_;
        }
        if (times_ > kDebounceThreshold) {
            times_ = 0;
            status_ = static_cast<uint8_t>(1 - status_);
        }
    }

    [[nodiscard]] uint8_t status() const { return status_; }

private:
    uint8_t status_ = 0;
    uint8_t times_ = 0;
};

} // namespace app

#endif // APP_INPUTDEBOUNCE_HPP
