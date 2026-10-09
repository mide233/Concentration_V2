#ifndef APP_PERSISTENCE_HPP
#define APP_PERSISTENCE_HPP

#include <cstdint>

namespace app {

/* 转换/校准结果；字段顺序与 Flash 持久化布局绑定，勿改。 */
struct ConversionValue {
    float rawValue;
    uint16_t uvLightLevel;
};

/* 一次检测的状态标志（数值沿用历史约定）。 */
enum class ConversionFlag : uint8_t {
    NotInitialized = 0,
    Ready = 1,
    Finished = 2,
    InProgress = 3,
};

/*
 * 校准值的 Flash 持久化（末页 0x0800FC00）。
 * 仅做存储，不含业务逻辑。
 */
class PersistentStore {
public:
    /* 读取校准值：magic 有效返回 true，否则写入默认值并返回 false。 */
    static bool load(ConversionValue &out);

    /* 保存校准值：仅当 flag 表示本次检测完成时才写入。 */
    static void save(const ConversionValue &value, ConversionFlag flag);

private:
    static bool hasValidMagic();
};

} // namespace app

#endif // APP_PERSISTENCE_HPP
