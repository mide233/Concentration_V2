#ifndef __Concentration_Conversion_H__
#define __Concentration_Conversion_H__

#include <stdint.h>
#include <stdbool.h>
typedef enum {
    ms = 1,
    s,
    us
} time_Unit_e;

typedef enum {
    ma = 1,
    ua,
    na
} current_Unit_e;
typedef enum {
    not_INIT,
    ready,
    finish,
    not_finish
} Conversion_flag_e;
typedef struct {
    int last_times;
    int times;
    uint32_t delta_time;
} test_t;
typedef struct {
    float Raw_value;
    uint16_t UVlight_level;
    // float value_B;
} Conversion_value_t;

// 定义带 magic 的存储结构体
typedef struct {
    uint32_t magic;          // 校验魔数
    Conversion_value_t data; // 实际有效数据
} StoredConversion_t;

typedef struct {
    float delta_time; // 上次至本次调用的时间间隔
    float Unit;
    float time_Unit;
    float detection_time;
    float once_detection_time; // 一次检测所需时间，单位：s
    float current_total;
    float current_start;
    Conversion_value_t Conversion_value;
    Conversion_flag_e Conversion_flag;
} Concentration_Conversion_t;

// float my_lg(float x);
// float my_ln(float x);

void Write_Conversion_Value(const Conversion_value_t *val, Concentration_Conversion_t *Concentration_Conversion);
uint8_t Read_Conversion_Value(Conversion_value_t *out);
float custom_exp10(float x);
void Concentration_Conversion_init(Concentration_Conversion_t *Concentration_Conversion, time_Unit_e time_Unit, float once_detection_time);
void Concentration_Conversion_updata(Concentration_Conversion_t *Concentration_Conversion, float current, float delta_time);
float get_Result(Concentration_Conversion_t *Concentration_Conversion);
bool Concentration_Conversion_calibration(Concentration_Conversion_t *Concentration_Conversion, float current, float delta_time);
void Concentration_Conversion_Reset(Concentration_Conversion_t *Concentration_Conversion);
float Get_Concentration_Conversion_Detection_Time(Concentration_Conversion_t *Concentration_Conversion);
#endif
