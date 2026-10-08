#include "Concentration_Conversion.h"
// #include "main.h"
#include "math.h"
#include "stm32f1xx_hal.h"

#define CONVERSION_DATA_ADDR 0x0800FC00

// 读取转换参数，成功返回 1，失败（未初始化或数据无效）返回 0
uint8_t Read_Conversion_Value(Conversion_value_t *out)
{
    StoredConversion_t *p = (StoredConversion_t *)CONVERSION_DATA_ADDR;
    if (p->magic == 0x12345678)
    {
        *out = p->data;
        return 1;
    }
    // 返回默认值（可根据需求修改）
    out->Raw_value = 0.3f;
    out->UVlight_level = 70;
    return 0;
}

// 写入转换参数到 Flash（掉电保存）
void Write_Conversion_Value(const Conversion_value_t *val, Concentration_Conversion_t *Concentration_Conversion)
{
    StoredConversion_t buffer;
    buffer.magic = 0x12345678;
    buffer.data = *val;
    if (Concentration_Conversion->Conversion_flag == finish)
    {
        HAL_FLASH_Unlock();

        // 检查是否需要擦除：地址全为 0xFFFFFFFF 或 magic 不匹配（即未写入或数据无效）
        uint32_t *checkAddr = (uint32_t *)CONVERSION_DATA_ADDR;
        if (*checkAddr == 0xFFFFFFFF || Read_Conversion_Value(NULL) == 1)
        {
            // 擦除整个页（1KB）
            FLASH_EraseInitTypeDef erase;
            erase.TypeErase = FLASH_TYPEERASE_PAGES;
            erase.PageAddress = CONVERSION_DATA_ADDR;
            erase.NbPages = 1;
            uint32_t pageError = 0;
            HAL_FLASHEx_Erase(&erase, &pageError);
        }

        // 按字（32位）写入结构体数据
        uint32_t *pSrc = (uint32_t *)&buffer;
        for (int i = 0; i < sizeof(StoredConversion_t) / 4; i++)
        {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              CONVERSION_DATA_ADDR + i * 4,
                              pSrc[i]);
        }

        HAL_FLASH_Lock();
    }
}
//// 计算自然对数 ln(x)
// float my_ln(float x) {
//     if (x <= 0) return 0; // 错误处理：x需为正数
//     float t = (x - 1) / (x + 1);
//     float t2 = t * t;
//     float t_power = t;
//     float ln_x = 0;
//     int coeff = 1;

//    for (int i = 0; i < 4; i++) {
//        ln_x += t_power / coeff;  // 累加泰勒级数项
//        coeff += 2;              // 系数更新：1, 3, 5, 7
//        t_power *= t2;           // t的幂次更新：t^3, t^5, t^7
//    }
//    return 2 * ln_x;
//}

//// 计算 lg(x)
// float my_lg(float x) {
//     return my_ln(x) / 2.302585092994046; // ln(10) ≈ 2.302585092994046
// }

// 近似计算10^x (x ∈ [-10, 10])
float custom_exp10(float x)
{
    // 1. 分离整数和小数部分
    int integer = (int)x;
    float fractional = x - integer;

    // 2. 计算整数部分: 10^integer
    float int_power = 1.0;
    if (integer > 0)
    {
        for (int i = 0; i < integer; i++)
            int_power *= 10.0;
    }
    else
    {
        for (int i = 0; i > integer; i--)
            int_power /= 10.0;
    }

    // 3. 计算小数部分: 10^fractional (使用多项式近似)
    // 在[0,1)区间内近似，使用4阶多项式拟合
    const float c0 = 0.9999999995;
    const float c1 = 2.302580022;
    const float c2 = 2.650910053;
    const float c3 = 5.330199229;

    float frac_power = c0 + fractional * c1 + fractional * fractional * c2 + fractional * fractional * fractional * c3;

    // 4. 组合结果
    return int_power * frac_power;
}

void Concentration_Conversion_init(Concentration_Conversion_t *Concentration_Conversion, time_Unit_e time_Unit, float once_detection_time)
{
    float c_Unit;
    Concentration_Conversion->once_detection_time = once_detection_time;
    if (time_Unit == s)
    {
        Concentration_Conversion->time_Unit = 1.0f;
    }
    if (time_Unit == ms)
    {
        Concentration_Conversion->time_Unit = 0.001f;
    }
    if (time_Unit == us)
    {
        Concentration_Conversion->time_Unit = 0.000001f;
    }
    if (Read_Conversion_Value(&Concentration_Conversion->Conversion_value))
    {
        Concentration_Conversion->Conversion_flag = ready;
    }
    else
    {
        Concentration_Conversion->Conversion_flag = not_INIT;
    }

    Concentration_Conversion->Unit = Concentration_Conversion->time_Unit * c_Unit;
}
void Concentration_Conversion_updata(Concentration_Conversion_t *Concentration_Conversion, float current, float delta_time)
{
    if (Concentration_Conversion->Conversion_flag == ready || Concentration_Conversion->Conversion_flag == not_INIT)
    {
        Concentration_Conversion->Conversion_flag = not_finish;
        Concentration_Conversion->current_start = current;
    }
    else if (Concentration_Conversion->Conversion_flag == not_finish)
    {
        Concentration_Conversion->current_total += (current - Concentration_Conversion->current_start) * delta_time;
        Concentration_Conversion->detection_time += delta_time * Concentration_Conversion->time_Unit;
        if (Concentration_Conversion->detection_time >= Concentration_Conversion->once_detection_time)
        {
            Concentration_Conversion->Conversion_flag = finish;
        }
    }
}
float get_Result(Concentration_Conversion_t *Concentration_Conversion)
{
    float Result = 0;
    if (Concentration_Conversion->Conversion_flag == finish)
    {
        // Q = Concentration_Conversion -> Conversion_value.Raw_value - Concentration_Conversion -> current_total;
        // value = (Q+Concentration_Conversion -> Conversion_value.value_A)/Concentration_Conversion -> Conversion_value.value_B;
        Result = Concentration_Conversion->current_total - Concentration_Conversion->Conversion_value.Raw_value;
        Concentration_Conversion->current_total = 0.0f;
        Concentration_Conversion->detection_time = 0.0f;
        Concentration_Conversion->Conversion_flag = ready;
    }
    return Result;
}
bool Concentration_Conversion_calibration(Concentration_Conversion_t *Concentration_Conversion, float current, float delta_time)
{
    Concentration_Conversion_updata(Concentration_Conversion, current, delta_time);
    if (Concentration_Conversion->Conversion_flag == finish)
    {
        Concentration_Conversion->Conversion_value.Raw_value = Concentration_Conversion->current_total;
        Concentration_Conversion->current_total = 0;
    }
    return Concentration_Conversion->Conversion_flag == finish;
}
void Concentration_Conversion_Reset(Concentration_Conversion_t *Concentration_Conversion)
{
    if (Concentration_Conversion->Conversion_flag != not_INIT)
        Concentration_Conversion->Conversion_flag = ready;
    Concentration_Conversion->current_total = 0;
    Concentration_Conversion->detection_time = 0;
}
float Get_Concentration_Conversion_Detection_Time(Concentration_Conversion_t *Concentration_Conversion)
{
    return Concentration_Conversion->detection_time;
}
