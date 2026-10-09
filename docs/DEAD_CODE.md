# 死代码归档（DEAD CODE ARCHIVE）

本文件集中保存重写过程中从源码移除的**死代码 / 注释掉的遗留代码**，并标注其**原本位置**与**移除原因**。
按既定决策：源码中不再保留这些块；需要时可从这里查回原文。

> 说明：仅整理“从未被引用/从未被执行”的代码与成块注释。
> **行为性缺陷**（如 R2 `c_Unit` 未初始化）按决策**不修**，仍保留在源码中（不做迁移）。
> 仍在运行但输出无消费者的代码（如 UART 收帧路径，R8）**保留在源码**，不在此列。

---

## 1. `Core/Src/Concentration_Conversion.cpp`（S2 删除，改为 `app/Measurement.cpp` + `app/Persistence.cpp`）

### 1.1 `custom_exp10()` —— 从未被调用
原位置：文件内函数定义（约在 `Read_Conversion_Value` 之后）。
```c
float custom_exp10(float x)
{
    /* 10^x：整数部分乘除 10，小数部分用四阶多项式逼近 */
    ...
}
```
移除原因：全仓无调用点（R6）。

### 1.2 `Get_Concentration_Conversion_Detection_Time()` —— 从未被调用
原位置：文件末尾。
```c
float Get_Concentration_Conversion_Detection_Time(Concentration_Conversion_t *cc)
{
    return cc->detection_time;
}
```
移除原因：全仓无调用点（R6）。

### 1.3 注释掉的 `my_ln` / `my_lg`
原位置：文件中部的成块注释。
```c
// float my_lg(float x) { ... }
// float my_ln(float x) { ... }
```
移除原因：历史遗留、未启用。

### 1.4 `get_Result()` 中被注释的备用公式
原位置：`get_Result()` 内部。
```c
// Result = current_total / Conversion_value.Raw_value;  // 备用换算公式
```
移除原因：历史遗留、未启用。

### 1.5 注释掉的 `#include "main.h"`
原位置：文件头部 includes。
```c
// #include "main.h"
```
移除原因：被注释且未启用。

### 1.6 结构体字段 `Concentration_Conversion_t::delta_time`
原位置：`Core/Inc/Concentration_Conversion.h`。
```c
float delta_time; // 上次至本次调用的时间间隔（字段本身从未被写入/读取）
```
移除原因：函数通过形参 `delta_time` 传参，字段从未使用。改为 C++ 类后不再保留该字段。

### 1.7 `Core/Inc/Concentration_Conversion.h` 中的未使用类型
`test_t`、`current_Unit_e`、注释掉的 `my_lg/my_ln` 原型、注释掉的 `value_B` 字段。均为未使用/历史遗留（R6）。

---

## 2. `Core/Inc/main.h`（S2 瘦身）

### 2.1 失效函数原型
```c
void Average_filtering(uint16_t *input_data, uint16_t num_channels, uint16_t total_samples, uint16_t *output_avg);
void DMA1_Channel1_IRQHandler(void);
void Concentration_Conversion_task(my_data_t *my_data);
void Concentration_Conversion_task_init(my_data_t *my_data);
```
移除原因：`Average_filtering` 已是 `App` 私有成员；`Concentration_Conversion_task*` 全仓无定义；`DMA1_Channel1_IRQHandler` 原型位于生成的 `stm32f1xx_it.c`。

### 2.2 未使用宏
```c
#define set_UVlight_level(my_data, level) ...
#define set_light_level(my_data, level) ...
#define UVlight_ON() ...
#define UVlight_OFF() ...
#define M_MIN(a,b) / M_MAX(a,b) / M_ABS(x) / M_CLAMP(x,min,max)
#define ADC_BAT_CHANNEL / ADC_INT_CHANNEL
#define UVlight_level_update(...) / DC_ctrl_ON() / DC_ctrl_OFF()
#define DETECTION_TIME / CLOSE_level / SW_STATUS / TILT_STATUS / KEY_STATUS / V_MAX / V_MIN
```
移除原因：`App.cpp` 已改用 `app::AppConfig.hpp` 常量与 `app::Hardware.hpp` 内联函数；其余无引用。仅保留 `RX_BUFFER_SIZE`。

### 2.3 迁移走的类型与状态
`BAT_status_e`、`work_status_e`、`BAT_t`、`hardware_status_t`、`my_data_t` 迁至 `Core/Inc/app/AppState.hpp`，并以 `enum class` / 结构体重命名（仅 `App.cpp` 使用，C 文件不使用）。

---

## 3. `Core/Src/main.c`

### 3.1 未使用全局
```c
uint8_t I = 0;                       // 全仓无引用
// uint32_t ndtr;                    // 注释掉的遗留
// extern I2C_HandleTypeDef hi2c1;  // 注释掉的遗留
```
### 3.2 注释掉的旧启动序列
```c
// HAL_ADC_Start_DMA(&hadc1, (uint32_t *)my_data.ADC_value[0], 20);
// HAL_DMA_Start_IT(&hdma_adc1, (uint32_t)&hadc1.Instance->DR, (uint32_t)my_data.ADC_value[0], 20);
// HAL_TIM_Base_Start_IT(&htim2);
// HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
```
移除原因：已迁移至 `App::init()`，注释块为遗留。

### 3.3 FIFO 用法（`main.c` 侧，S4 移除）
```c
#define MSG_SIZE 32
static fifo_data_t msg_buf[MSG_SIZE];
fifo_lockfree_t msg_fifo = FIFO_LOCKFREE_INIT(msg_fifo, msg_buf, MSG_SIZE);
#include "FIFO_LOCKFREE.h"   // 仅为上述对象而包含
```
移除原因：`msg_fifo` / `msg_buf` 全仓无读写点，`FIFO_LOCKFREE` 整模块未被引用（R6）。S4 从 `main.c` 移除其用法与 include；模块文件本身按计划在 S7 统一删除。

---

## 4. `Core/Src/OLED.cpp` / `Core/Inc/OLED.h`（S5 删除，改为 `app/OledPanel.cpp` + `app/Display.cpp`）

### 4.1 `Phys_DrawCharNormal()` —— 仅被 `TEXT_MODE==1` 的水平文字路径使用
原位置：`OLED.cpp` 内字符绘制函数区。
```c
/* 正立字符绘制，仅 TEXT_MODE==1 使用；当前 TEXT_MODE==0（侧躺），从未被调用 */
```
移除原因：`TEXT_MODE` 固定为 0，水平文字路径整体不可达（R6）。

### 4.2 `Phys_DrawHorizontalString()` —— 从未被调用
原位置：`OLED.cpp` 文件内函数定义（`Phys_DrawCharNormal` 之后）。
```c
/* 水平排布字符串，仅 TEXT_MODE==1 使用 */
```
移除原因：仅服务于未启用的 `TEXT_MODE==1` 路径；编译器曾报 `defined but not used`（R6）。

### 4.3 `TEXT_MODE==1` 水平分支与相关宏
原位置：`OLED.cpp::update()` 内的 `if (TEXT_MODE == 1) ... else ...` 分支，以及 `OLED.h` 中的
`TEXT_MODE` / `TEXT_MIRROR_H` / `TEXT_MIRROR_V` / `TEXT_REVERSE_ORDER` 调试宏。
移除原因：固定走侧躺竖直路径后，水平分支不可达；参数已固化为 `Display.cpp` 中的常量。

### 4.4 注释掉的旧蓝牙位图
原位置：`OLED.cpp` 蓝牙图标区的成块注释。
```c
/* 以下为旧版 bluetooth 位图，疑似废弃，待人工确认 */
// const uint8_t bluetooth_old[16][20] = { ... };
```
移除原因：历史遗留、未启用。

---

## 5. `Core/Inc/FIFO_LOCKFREE.h` / `Core/Src/FIFO_LOCKFREE.cpp`（S7 删除）

### 5.1 整模块
```c
typedef uint16_t fifo_data_t;
typedef struct { fifo_data_t *buf; uint_fast8_t size; volatile uint_fast8_t head; volatile uint_fast8_t tail; } fifo_lockfree_t;
#define FIFO_LOCKFREE_INIT(name, _buf, _size) { ... }
void fifo_lockfree_reset / is_empty / is_full / put / get / available(...);
```
原位置：整文件。
移除原因：全仓无任何 `fifo_lockfree_*` 调用点；`main.c` 中的 `msg_fifo` / `msg_buf` 用法已在 S4 移除（R6）。

---

## 6. 保留但已记录（未移除）

- **`battery_level(float)`（`App.cpp`）**：作为外部 C 符号保留，但当前无调用点（`battery` 显示始终为 0）。保留以维持导出符号集合。
- **UART 收帧路径**：有实际 DMA 副作用（R8），已迁至 `Core/Src/app/UartReceiver.cpp`，保留。
- **`Measurement::unit_` / `c_Unit`（R2）**：按“仅修 R3/R4”的决策保留原行为。
