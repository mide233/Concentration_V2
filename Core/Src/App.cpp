/**
 ******************************************************************************
 * @file    App.cpp
 * @brief   应用层逻辑（C++）：状态机、ADC 完成回调、电量与进度计算。
 *
 * 由 main.c 与 HAL 回调驱动；对外仅暴露 App.h 中的 C 接口以及 HAL 的
 * HAL_ADC_ConvCpltCallback。无堆分配、无异常/RTTI、无全局构造函数。
 ******************************************************************************
 */

#include "App.h"

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "OLED.h"

extern "C" {
/* main.c 中定义的 UART 接收缓冲区（stm32f1xx_it.c 亦以 extern 引用） */
extern uint8_t rx_dma_buffer[];
}

namespace {

/*
 * 应用运行状态与业务逻辑。
 * 原先散落在 main.c 的全局 my_data / hardware_status 及其处理函数，
 * 现统一封装为该类的成员与方法，经 g_app 单例访问。
 */
class App
{
public:
    void init();
    void updateDisplay();
    void onAdcComplete();

private:
    void Average_filtering(uint16_t *input_data, uint16_t num_channels, uint16_t total_samples, uint16_t *output_avg);
    void Concentration_Conversion_task();
    void CC_set_work_status();

    hardware_status_t hw; // 按键/开关硬件状态（原全局 hardware_status）
    my_data_t data;       // 应用运行数据（原全局 my_data）
};

} // namespace

// 文件内唯一实例：常量静态初始化（无动态构造、无 .init_array 项）。
constinit App g_app;

void App::init()
{
    // 1. 启动DMA接收，让数据在后台自动搬运到 rx_dma_buffer[reference:22]
    HAL_UART_Receive_DMA(&huart1, rx_dma_buffer, RX_BUFFER_SIZE);

    // 2. 开启USART1的空闲中断[reference:23]
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    UVlight_level_update(CLOSE_level);
    DC_ctrl_OFF();
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&data.ADC_value[0], 20);
    HAL_Delay(10);

    Concentration_Conversion_init(&data.Concentration_Conversion, ms, 100.0f);
    data.hardware_status = &hw;
    data.hardware_status->SW_status = 1 - SW_STATUS;
    data.hardware_status->TILT_status = 1 - TILT_STATUS;
    data.hardware_status->KEY_status = 1 - KEY_STATUS;
}

void App::updateDisplay()
{
    OLED_Update(data.BAT.battery_level, 1, data.progress);
}

void App::Average_filtering(uint16_t *input_data, uint16_t num_channels, uint16_t total_samples, uint16_t *output_avg)
{
    uint16_t samples_per_channel = total_samples / num_channels;

    for (uint16_t ch = 0; ch < num_channels; ch++)
    {
        uint32_t sum = 0;
        for (uint16_t s = 0; s < samples_per_channel; s++)
        {
            sum += input_data[ch + s * num_channels];
        }
        output_avg[ch] = (uint16_t)(sum / samples_per_channel);
    }
}

void App::Concentration_Conversion_task()
{
    if (data.work_status == calibration)
    {
        Concentration_Conversion_calibration(&data.Concentration_Conversion, data.ADC_INT, DETECTION_TIME);
        if (data.Concentration_Conversion.Conversion_flag == not_finish)
        {
            UVlight_level_update(data.Concentration_Conversion.Conversion_value.UVlight_level);
        }
    }
    else if (data.work_status == working)
    {
        Concentration_Conversion_update(&data.Concentration_Conversion, data.ADC_INT, DETECTION_TIME);
    }
    else if (data.work_status == SAVE)
    {
        Write_Conversion_Value(&data.Concentration_Conversion.Conversion_value, &data.Concentration_Conversion);
        data.work_status = readay;
    }
    else if (data.work_status == INIT)
    {
        ;
    }
    if (data.work_status == calibration || data.work_status == working)
    {
        data.progress = (uint8_t)(data.Concentration_Conversion.detection_time);
    }
    else if (data.BAT.BAT_status != NORMAL)
    {
        data.progress = (uint8_t)(data.BAT.adc_BAT / V_MAX * 100);
    }
}

void App::CC_set_work_status()
{
    /* ==================== 1. 意图状态转换 ==================== */
    switch (data.work_status)
    {
    case readay:
        if (data.hope_status == working && data.Concentration_Conversion.Conversion_flag != finish)
        {
            Concentration_Conversion_Reset(&data.Concentration_Conversion);
            UVlight_level_update(data.Concentration_Conversion.Conversion_value.UVlight_level);
            DC_ctrl_ON();
            data.work_status = working;
        }
        else if (data.hope_status == calibration && data.Concentration_Conversion.Conversion_flag != finish)
        {
            Concentration_Conversion_Reset(&data.Concentration_Conversion);
            UVlight_level_update(data.Concentration_Conversion.Conversion_value.UVlight_level);
            DC_ctrl_ON();
            data.work_status = calibration;
        }
        else if (data.hope_status == SAVE)
        {
            data.work_status = SAVE;
        }
        break;

    case working:
    case calibration:
    case INIT:
        if (data.hope_status == readay && data.Concentration_Conversion.Conversion_flag == finish)
        {
            data.result = get_Result(&data.Concentration_Conversion);
            data.work_status = readay;
        }
        else if (data.hope_status != readay && data.Concentration_Conversion.Conversion_flag == finish)
        {
            UVlight_level_update(CLOSE_level);
            DC_ctrl_OFF();
        }
        else if (data.hope_status == readay && data.Concentration_Conversion.Conversion_flag == not_finish)
        {
            UVlight_level_update(CLOSE_level);
            DC_ctrl_OFF();
            data.work_status = readay;
        }
        else if (data.hope_status == readay && data.Concentration_Conversion.Conversion_flag == ready)
        {
            UVlight_level_update(CLOSE_level);
            DC_ctrl_OFF();
            data.work_status = readay;
        }
        if (data.hardware_status->TILT_status == TILT_STATUS)
            data.work_status = err_TILT;
        else if (data.hardware_status->SW_status == SW_STATUS)
            data.work_status = err_open;
        else if (data.hardware_status->KEY_status == KEY_STATUS)
            data.work_status = err_no_cap;
        break;

    case err_TILT:
    case err_open:
    case err_low_pow:
    case err_no_cap:
        if (data.hope_status == readay)
        {
            data.work_status = readay;
        }
        break;

    default:
        break;
    }

    /* ==================== 3. 低电量检测 ==================== */
    // if (my_data->BAT.battery_level <= 1 &&
    //     my_data->work_status != working &&
    //     my_data->work_status != calibration &&
    //     my_data->BAT.BAT_status != CHRG)
    // {
    //     my_data->work_status = err_low_pow;
    // }

    /* ==================== 4. 错误恢复 ==================== */
    if (data.work_status == err_low_pow &&
        (data.BAT.battery_level > 2 || data.BAT.BAT_status == SHDBY))
    {
        data.work_status = readay;
    }
    if (data.work_status == err_open &&
        data.hardware_status->SW_status != SW_STATUS)
    {
        data.work_status = readay;
    }
    if (data.work_status == err_TILT &&
        data.hardware_status->TILT_status != TILT_STATUS)
    {
        data.work_status = readay;
    }
    if (data.work_status == err_no_cap &&
        data.hardware_status->KEY_status != KEY_STATUS)
    {
        data.work_status = readay;
    }

    /* ==================== 5. 电池状态更新 ==================== */
    if (HAL_GPIO_ReadPin(SHDBY_GPIO_Port, SHDBY_Pin) == GPIO_PIN_RESET)
    {
        data.BAT.BAT_status = SHDBY;
    }
    else if (HAL_GPIO_ReadPin(CHRG_GPIO_Port, CHRG_Pin) == GPIO_PIN_RESET)
    {
        data.BAT.BAT_status = CHRG;
    }
    else
    {
        data.BAT.BAT_status = NORMAL;
    }
}

/* 电量等级换算：保留原 main.c 的外部 C 符号（main.h 已声明），不作为 App 成员。 */
extern "C" int battery_level(float voltage)
{
    if (voltage >= V_MAX)
        return 5;
    if (voltage <= V_MIN)
        return 0;

    // 计算线性比例并四舍五入到最近整数
    float ratio = (voltage - V_MIN) / (V_MAX - V_MIN);
    int level = (int)(ratio * 5 + 0.5f); // 1~5

    // 安全检查
    if (level < 0)
        level = 0;
    if (level > 5)
        level = 5;
    return level;
}

void App::onAdcComplete()
{
    Average_filtering(data.ADC_value, 2, 20, data.ADC_avg);
    CC_set_work_status();
    data.ADC_INT = data.ADC_avg[ADC_INT_CHANNEL];
    data.BAT.adc_BAT = data.ADC_avg[ADC_BAT_CHANNEL];
    Concentration_Conversion_task();
    if (data.work_status == working || data.work_status == calibration)
    {
        if (HAL_GPIO_ReadPin(SW_GPIO_Port, SW_Pin) != data.hardware_status->SW_status)
        {
            data.hardware_status->SW_times++;
        }
        if (HAL_GPIO_ReadPin(TILT_GPIO_Port, TILT_Pin) != data.hardware_status->TILT_status)
        {
            data.hardware_status->TILT_times++;
        }
        if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) != data.hardware_status->KEY_status)
        {
            data.hardware_status->KEY_times++;
        }
        if (data.hardware_status->SW_times > 5)
        {
            data.hardware_status->SW_times = 0;
            data.hardware_status->SW_status = 1 - data.hardware_status->SW_status;
        }
        if (data.hardware_status->TILT_times > 5)
        {
            data.hardware_status->TILT_times = 0;
            data.hardware_status->TILT_status = 1 - data.hardware_status->TILT_status;
        }
        if (data.hardware_status->KEY_times > 5)
        {
            data.hardware_status->KEY_times = 0;
            data.hardware_status->KEY_status = 1 - data.hardware_status->KEY_status;
        }
    }
}

extern "C" void App_Init(void)
{
    g_app.init();
}

extern "C" void App_UpdateDisplay(void)
{
    g_app.updateDisplay();
}

/* HAL 回调入口：与原先在 main.c 中的函数名/签名一致（C 链接）。 */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    g_app.onAdcComplete();
}
