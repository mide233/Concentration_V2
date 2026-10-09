#ifndef APP_APPCONFIG_HPP
#define APP_APPCONFIG_HPP

#include <cstdint>

/*
 * 应用层编译期常量。
 * 原为 main.h 中的宏，集中到此以消除魔法数字并保持单一来源。
 */

namespace app {

/* ADC：一次搬运 20 个样本、2 个通道（光电流 + 电池分压），每通道平均 10 次 */
inline constexpr std::uint16_t kAdcSampleCount  = 20;
inline constexpr std::uint16_t kAdcChannelCount = 2;
inline constexpr std::uint16_t kAdcIntChannel   = 0; // 光电流
inline constexpr std::uint16_t kAdcBatChannel   = 1; // 电池分压

/* 电池电压映射区间（V） */
inline constexpr float kBatteryVMax = 4.2f;
inline constexpr float kBatteryVMin = 3.0f;

/* 检测 / 校准参数 */
inline constexpr float         kDetectionDeltaTime = 10.0f;  // 每次迭代的时间步（原 DETECTION_TIME）
inline constexpr float         kOnceDetectionTime  = 100.0f; // 一次检测所需时间
inline constexpr std::uint16_t kUvCloseLevel       = 0;      // UV 关闭亮度（原 CLOSE_level）

/* 按键消抖：连续读到与当前状态不同的次数超过该值才翻转 */
inline constexpr std::uint8_t kDebounceThreshold = 5;

} // namespace app

#endif /* APP_APPCONFIG_HPP */
