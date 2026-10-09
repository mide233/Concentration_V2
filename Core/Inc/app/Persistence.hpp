#ifndef APP_PERSISTENCE_HPP
#define APP_PERSISTENCE_HPP

#include <cstddef>
#include <cstdint>

#include "stm32f1xx_hal.h"

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

namespace detail {

inline constexpr uint32_t kDataAddress = 0x0800FC00u; // 64 KB Flash 末页
inline constexpr uint32_t kMagic = 0x12345678u;
inline constexpr float kDefaultRawValue = 0.3f;
inline constexpr uint16_t kDefaultUvLightLevel = 70;

/* Flash 中实际存放的镜像（magic + 数据），布局冻结。 */
struct StoredConversion {
    uint32_t magic;
    ConversionValue data;
};

/* Flash 解锁/加锁的 RAII 守卫。 */
class FlashWriteGuard {
public:
    FlashWriteGuard() { HAL_FLASH_Unlock(); }
    ~FlashWriteGuard() { HAL_FLASH_Lock(); }
    FlashWriteGuard(const FlashWriteGuard&) = delete;
    FlashWriteGuard& operator=(const FlashWriteGuard&) = delete;
};

static_assert(sizeof(ConversionValue) == 8, "ConversionValue layout");
static_assert(sizeof(StoredConversion) == 12, "StoredConversion layout");
static_assert(offsetof(StoredConversion, magic) == 0, "magic offset");
static_assert(offsetof(StoredConversion, data) == 4, "data offset");
static_assert(static_cast<uint8_t>(ConversionFlag::NotInitialized) == 0, "flag 0");
static_assert(static_cast<uint8_t>(ConversionFlag::Ready) == 1, "flag 1");
static_assert(static_cast<uint8_t>(ConversionFlag::Finished) == 2, "flag 2");
static_assert(static_cast<uint8_t>(ConversionFlag::InProgress) == 3, "flag 3");

} // namespace detail

/*
 * 校准值的 Flash 持久化（末页 0x0800FC00）。
 * 仅做存储，不含业务逻辑。
 */
class PersistentStore {
public:
    /* 读取校准值：magic 有效返回 true，否则写入默认值并返回 false。 */
    static bool load(ConversionValue& out) {
        const auto* stored =
            reinterpret_cast<const detail::StoredConversion*>(detail::kDataAddress);
        if (stored->magic == detail::kMagic) {
            out = stored->data;
            return true;
        }
        out.rawValue = detail::kDefaultRawValue;
        out.uvLightLevel = detail::kDefaultUvLightLevel;
        return false;
    }

    /* 保存校准值：仅当 flag 表示本次检测完成时才写入。 */
    static void save(const ConversionValue& value, ConversionFlag flag) {
        if (flag != ConversionFlag::Finished) {
            return;
        }

        detail::StoredConversion buffer{};
        buffer.magic = detail::kMagic;
        buffer.data = value;

        detail::FlashWriteGuard flashGuard;

        // 首次写入时该页为全 0xFF（无需擦除）；否则必须先擦除整页。
        // 注意：这里用 hasValidMagic() 判读首字，避免历史上的 Read_Conversion_Value(NULL)
        // 空指针（R3）。
        const auto* check = reinterpret_cast<const uint32_t*>(detail::kDataAddress);
        if (*check == 0xFFFFFFFFu || hasValidMagic()) {
            FLASH_EraseInitTypeDef erase{};
            erase.TypeErase = FLASH_TYPEERASE_PAGES;
            erase.PageAddress = detail::kDataAddress;
            erase.NbPages = 1;
            uint32_t pageError = 0;
            HAL_FLASHEx_Erase(&erase, &pageError);
        }

        constexpr size_t kWordCount = sizeof(detail::StoredConversion) / sizeof(uint32_t);
        const auto* src = reinterpret_cast<const uint32_t*>(&buffer);
        for (size_t i = 0; i < kWordCount; ++i) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, detail::kDataAddress + i * 4u, src[i]);
        }
    }

private:
    static bool hasValidMagic() {
        const auto* stored =
            reinterpret_cast<const detail::StoredConversion*>(detail::kDataAddress);
        return stored->magic == detail::kMagic;
    }
};

} // namespace app

#endif // APP_PERSISTENCE_HPP
