#include "app/Persistence.hpp"

#include <cstddef>

#include "stm32f1xx_hal.h"

namespace app
{
    namespace
    {

        constexpr uint32_t kDataAddress         = 0x0800FC00u; // 64 KB Flash 末页
        constexpr uint32_t kMagic               = 0x12345678u;
        constexpr float kDefaultRawValue        = 0.3f;
        constexpr uint16_t kDefaultUvLightLevel = 70;

        /* Flash 中实际存放的镜像（magic + 数据），布局冻结。 */
        struct StoredConversion {
            uint32_t magic;
            ConversionValue data;
        };

        /* Flash 解锁/加锁的 RAII 守卫。 */
        class FlashWriteGuard
        {
        public:
            FlashWriteGuard()
            { HAL_FLASH_Unlock(); }
            ~FlashWriteGuard()
            { HAL_FLASH_Lock(); }
            FlashWriteGuard(const FlashWriteGuard &)            = delete;
            FlashWriteGuard &operator=(const FlashWriteGuard &) = delete;
        };

        static_assert(sizeof(ConversionValue) == 8, "ConversionValue layout");
        static_assert(sizeof(StoredConversion) == 12, "StoredConversion layout");
        static_assert(offsetof(StoredConversion, magic) == 0, "magic offset");
        static_assert(offsetof(StoredConversion, data) == 4, "data offset");
        static_assert(static_cast<uint8_t>(ConversionFlag::NotInitialized) == 0, "flag 0");
        static_assert(static_cast<uint8_t>(ConversionFlag::Ready) == 1, "flag 1");
        static_assert(static_cast<uint8_t>(ConversionFlag::Finished) == 2, "flag 2");
        static_assert(static_cast<uint8_t>(ConversionFlag::InProgress) == 3, "flag 3");

    } // namespace

    bool PersistentStore::hasValidMagic()
    {
        const auto *stored = reinterpret_cast<const StoredConversion *>(kDataAddress);
        return stored->magic == kMagic;
    }

    bool PersistentStore::load(ConversionValue &out)
    {
        const auto *stored = reinterpret_cast<const StoredConversion *>(kDataAddress);
        if (stored->magic == kMagic) {
            out = stored->data;
            return true;
        }
        out.rawValue     = kDefaultRawValue;
        out.uvLightLevel = kDefaultUvLightLevel;
        return false;
    }

    void PersistentStore::save(const ConversionValue &value, ConversionFlag flag)
    {
        if (flag != ConversionFlag::Finished) {
            return;
        }

        StoredConversion buffer{};
        buffer.magic = kMagic;
        buffer.data  = value;

        FlashWriteGuard flashGuard;

        // 首次写入时该页为全 0xFF（无需擦除）；否则必须先擦除整页。
        // 注意：这里用 hasValidMagic() 判读首字，避免历史上的 Read_Conversion_Value(NULL) 空指针（R3）。
        const auto *check = reinterpret_cast<const uint32_t *>(kDataAddress);
        if (*check == 0xFFFFFFFFu || hasValidMagic()) {
            FLASH_EraseInitTypeDef erase{};
            erase.TypeErase    = FLASH_TYPEERASE_PAGES;
            erase.PageAddress  = kDataAddress;
            erase.NbPages      = 1;
            uint32_t pageError = 0;
            HAL_FLASHEx_Erase(&erase, &pageError);
        }

        constexpr size_t kWordCount = sizeof(StoredConversion) / sizeof(uint32_t);
        const auto *src             = reinterpret_cast<const uint32_t *>(&buffer);
        for (size_t i = 0; i < kWordCount; ++i) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, kDataAddress + i * 4u, src[i]);
        }
    }

} // namespace app
