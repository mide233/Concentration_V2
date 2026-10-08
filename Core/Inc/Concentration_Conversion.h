#ifndef CONCENTRATION_CONVERSION_H
#define CONCENTRATION_CONVERSION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 时间单位（换算系数在 Concentration_Conversion_init 中设置） */
typedef enum {
    ms = 1,
    s,
    us
} time_Unit_e;

/* 电流单位（当前未被使用，保留待用） */
typedef enum {
    ma = 1,
    ua,
    na
} current_Unit_e;

/* 转换状态标志 */
typedef enum {
    not_INIT,  // 未初始化（Flash 无有效数据）
    ready,     // 就绪，可开始一次检测
    finish,    // 本次检测完成
    not_finish // 检测进行中
} Conversion_flag_e;

/* 测试用结构体（当前未被使用，保留待用） */
typedef struct {
    int last_times;
    int times;
    uint32_t delta_time;
} test_t;

/* 转换结果数据 */
typedef struct {
    float Raw_value;        // 原始（校准）基准值
    uint16_t UVlight_level; // UV 灯亮度等级
    // float value_B;       // [已注释] 历史遗留字段，疑似废弃，待人工确认
} Conversion_value_t;

/* Flash 持久化结构：带 magic 校验 */
typedef struct {
    uint32_t magic;          // 校验魔数（0x12345678 视为有效）
    Conversion_value_t data; // 实际有效数据
} StoredConversion_t;

/* 浓度/转换运算状态 */
typedef struct {
    float delta_time;                    // 上次至本次调用的时间间隔
    float Unit;                          // [疑似废弃] 由未初始化的 c_Unit 计算，全仓只写不读，见风险 R2
    float time_Unit;                     // 时间单位换算系数
    float detection_time;                // 当前累计检测时间
    float once_detection_time;           // 一次检测所需时间（单位：s）
    float current_total;                 // 电流对时间的累计量
    float current_start;                 // 本次检测的起始电流
    Conversion_value_t Conversion_value; // 转换结果数据
    Conversion_flag_e Conversion_flag;   // 状态标志
} Concentration_Conversion_t;

// [已注释] 以下为历史遗留的对数函数声明，疑似废弃，待人工确认（保留原样，勿删）
// float my_lg(float x);
// float my_ln(float x);

void Write_Conversion_Value(const Conversion_value_t *val, Concentration_Conversion_t *cc);
uint8_t Read_Conversion_Value(Conversion_value_t *out);
float custom_exp10(float x);
void Concentration_Conversion_init(Concentration_Conversion_t *cc, time_Unit_e time_Unit, float once_detection_time);
void Concentration_Conversion_update(Concentration_Conversion_t *cc, float current, float delta_time);
float get_Result(Concentration_Conversion_t *cc);
bool Concentration_Conversion_calibration(Concentration_Conversion_t *cc, float current, float delta_time);
void Concentration_Conversion_Reset(Concentration_Conversion_t *cc);
float Get_Concentration_Conversion_Detection_Time(Concentration_Conversion_t *cc);

#ifdef __cplusplus
}
#endif

#endif
