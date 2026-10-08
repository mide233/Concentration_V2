/*
 * 浓度/转换运算与参数持久化。
 * 注意：校准参数保存在 Flash 绝对地址 0x0800FC00（64 KB Flash 的最后一页），
 *       该页未在链接脚本中保留，详见 AGENTS.md 风险 R4。
 */
#include "Concentration_Conversion.h"
// #include "main.h" // [已注释] 原始包含，疑似不再需要，待人工确认
#include "math.h"
#include "stm32f1xx_hal.h"

#define CONVERSION_DATA_ADDR 0x0800FC00 // 持久化数据所在 Flash 页的起始地址

/*
 * 功能：从 Flash 读取转换参数
 * 参数：out - 输出参数（读取结果或默认值）
 * 返回：1 读取成功（magic 校验通过）；0 数据无效（已填入默认值）
 * 注意：out 不可为 NULL（见风险 R3）。
 */
uint8_t Read_Conversion_Value(Conversion_value_t *out)
{
    StoredConversion_t *stored = (StoredConversion_t *)CONVERSION_DATA_ADDR;
    if (stored->magic == 0x12345678) // magic 校验通过
    {
        *out = stored->data;
        return 1;
    }
    // magic 无效：返回默认值（数值可按需求调整）
    out->Raw_value = 0.3f;
    out->UVlight_level = 70;
    return 0;
}

/*
 * 功能：将转换参数写入 Flash（掉电保存）
 * 参数：val - 待写入的数据；cc - 携带状态标志
 * 说明：仅当状态标志为 finish 时才执行写入。
 * 风险：R3 —— 本函数内以 NULL 调用 Read_Conversion_Value，
 *       当 Flash 中已存在有效 magic 时会解引用空指针。仅记录，未修改。
 */
void Write_Conversion_Value(const Conversion_value_t *val, Concentration_Conversion_t *cc)
{
    StoredConversion_t buffer;
    buffer.magic = 0x12345678;
    buffer.data = *val;
    if (cc->Conversion_flag == finish)
    {
        HAL_FLASH_Unlock();

        // 判断是否需要先擦除：该地址尚未写入（全 0xFF）或已存在有效数据
        uint32_t *checkAddr = (uint32_t *)CONVERSION_DATA_ADDR;
        if (*checkAddr == 0xFFFFFFFF || Read_Conversion_Value(NULL) == 1)
        {
            // 擦除该页（1 KB）
            FLASH_EraseInitTypeDef erase;
            erase.TypeErase = FLASH_TYPEERASE_PAGES;
            erase.PageAddress = CONVERSION_DATA_ADDR;
            erase.NbPages = 1;
            uint32_t pageError = 0;
            HAL_FLASHEx_Erase(&erase, &pageError);
        }

        // 按字（32 位）写入结构体数据
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

// [已注释] 以下为历史遗留的对数函数实现，疑似废弃，待人工确认（保留原样，勿删）
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
//}

/*
 * 功能：近似计算 10^x（x ∈ [-10, 10]）
 * 说明：整数部分用循环累乘/累除，小数部分用 4 阶多项式拟合。
 */
float custom_exp10(float x)
{
    // 1. 分离整数部分与小数部分
    int integer = (int)x;
    float fractional = x - integer;

    // 2. 计算整数部分：10^integer
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

    // 3. 计算小数部分：10^fractional（在 [0,1) 上用 4 阶多项式近似）
    const float c0 = 0.9999999995;
    const float c1 = 2.302580022;
    const float c2 = 2.650910053;
    const float c3 = 5.330199229;

    float frac_power = c0 + fractional * c1 + fractional * fractional * c2 + fractional * fractional * fractional * c3;

    // 4. 组合整数与小数部分的结果
    return int_power * frac_power;
}

/*
 * 功能：初始化转换状态机
 * 参数：cc - 状态对象；time_Unit - 时间单位；
 *       once_detection_time - 一次检测所需时间
 * 说明：从 Flash 读取校准参数；读取成功置 ready，否则置 not_INIT。
 * 风险：R2 —— 末行使用未初始化的局部变量 c_Unit（UB），且字段 Unit 全仓只写不读。仅记录，未修改。
 */
void Concentration_Conversion_init(Concentration_Conversion_t *cc, time_Unit_e time_Unit, float once_detection_time)
{
    float c_Unit; // [!] 未初始化，见风险 R2
    cc->once_detection_time = once_detection_time;
    if (time_Unit == s)
    {
        cc->time_Unit = 1.0f;
    }
    if (time_Unit == ms)
    {
        cc->time_Unit = 0.001f;
    }
    if (time_Unit == us)
    {
        cc->time_Unit = 0.000001f;
    }
    if (Read_Conversion_Value(&cc->Conversion_value))
    {
        cc->Conversion_flag = ready;
    }
    else
    {
        cc->Conversion_flag = not_INIT;
    }

    cc->Unit = cc->time_Unit * c_Unit; // [!] c_Unit 未初始化，见风险 R2
}

/*
 * 功能：更新转换累加量（在检测过程中周期调用）
 * 参数：current - 当前电流；delta_time - 距上次调用的时间间隔
 * 说明：首次进入（ready/not_INIT）记录起始电流；not_finish 期间累加电流并对时间积分；
 *       达到 once_detection_time 后置 finish。
 */
void Concentration_Conversion_updata(Concentration_Conversion_t *cc, float current, float delta_time)
{
    if (cc->Conversion_flag == ready || cc->Conversion_flag == not_INIT)
    {
        cc->Conversion_flag = not_finish;
        cc->current_start = current;
    }
    else if (cc->Conversion_flag == not_finish)
    {
        cc->current_total += (current - cc->current_start) * delta_time;
        cc->detection_time += delta_time * cc->time_Unit;
        if (cc->detection_time >= cc->once_detection_time)
        {
            cc->Conversion_flag = finish;
        }
    }
}

/*
 * 功能：取转换结果
 * 返回：finish 时为累计量减去原始基准值之差，否则返回 0
 * 说明：返回后将累计量与检测时间清零，并把状态复位为 ready。
 * 注意：函数内保留了一段注释掉的备选计算公式，疑似废弃，待人工确认。
 */
float get_Result(Concentration_Conversion_t *cc)
{
    float Result = 0;
    if (cc->Conversion_flag == finish)
    {
        // [已注释] 备选计算公式，疑似旧实现，待人工确认（保留原样，勿删）
        // Q = Concentration_Conversion -> Conversion_value.Raw_value - Concentration_Conversion -> current_total;
        // value = (Q+Concentration_Conversion -> Conversion_value.value_A)/Concentration_Conversion -> Conversion_value.value_B;
        Result = cc->current_total - cc->Conversion_value.Raw_value;
        cc->current_total = 0.0f;
        cc->detection_time = 0.0f;
        cc->Conversion_flag = ready;
    }
    return Result;
}

/*
 * 功能：执行一次校准
 * 参数：current - 当前电流；delta_time - 距上次调用的时间间隔
 * 返回：true 表示本次校准完成
 * 说明：内部调用 updata 推进；完成后把累计量写入 Raw_value 作为基准。
 */
bool Concentration_Conversion_calibration(Concentration_Conversion_t *cc, float current, float delta_time)
{
    Concentration_Conversion_updata(cc, current, delta_time);
    if (cc->Conversion_flag == finish)
    {
        cc->Conversion_value.Raw_value = cc->current_total;
        cc->current_total = 0;
    }
    return cc->Conversion_flag == finish;
}

/*
 * 功能：复位转换状态
 * 说明：非 not_INIT 时把状态置为 ready，并清零累计量与检测时间。
 */
void Concentration_Conversion_Reset(Concentration_Conversion_t *cc)
{
    if (cc->Conversion_flag != not_INIT)
        cc->Conversion_flag = ready;
    cc->current_total = 0;
    cc->detection_time = 0;
}

/*
 * 功能：读取当前累计检测时间
 */
float Get_Concentration_Conversion_Detection_Time(Concentration_Conversion_t *cc)
{
    return cc->detection_time;
}
