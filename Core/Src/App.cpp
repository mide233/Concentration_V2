/**
 ******************************************************************************
 * @file    App.cpp
 * @brief   应用层逻辑（C++）：状态机、ADC 完成回调、电量与进度计算。
 *
 * 由 main.c 与 HAL 回调驱动；对外仅暴露 App.h 中的 C 接口以及 HAL 的
 * HAL_ADC_ConvCpltCallback。无堆分配、无异常/RTTI。
 ******************************************************************************
 */

#include "App.h"

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"

#include "app/AppConfig.hpp"
#include "app/AppState.hpp"
#include "app/Display.hpp"
#include "app/Hardware.hpp"

extern "C" {
/* main.c 中定义的 UART 接收缓冲区（stm32f1xx_it.c 亦以 extern 引用） */
extern uint8_t rx_dma_buffer[];
}

namespace app {
namespace {

/*
 * 应用运行状态与业务逻辑。
 * 原先散落在 main.c 的全局 my_data / hardware_status 及其处理函数，
 * 现统一封装为该类的成员与方法，经 g_app 单例访问。
 */
class App {
public:
    void init();
    void updateDisplay();
    void onAdcComplete();

private:
    void averageFiltering(uint16_t *input, uint16_t numChannels, uint16_t totalSamples, uint16_t *out);
    void runMeasurementTask();
    void updateWorkStatus();

    AppData data_;
    Display display_;
};

} // namespace

// 文件内唯一实例。
App g_app;

void App::init()
{
    // 启动 UART DMA 接收与空闲中断。
    HAL_UART_Receive_DMA(&huart1, rx_dma_buffer, RX_BUFFER_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    setUvLevel(kUvCloseLevel);
    dcCtrlOff();
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

    HAL_ADC_Start_DMA(&hadc1, reinterpret_cast<uint32_t *>(&data_.adcValue[0]), kAdcSampleCount);
    HAL_Delay(10);

    data_.measurement.init(TimeUnit::Milliseconds, kOnceDetectionTime);

    // 初始输入状态取反（0 表示错误态，与历史 SW/TILT/KEY_STATUS 一致）。
    data_.input.sw.begin(1);
    data_.input.tilt.begin(1);
    data_.input.key.begin(1);

    // 初始化显示（原 main.c 的 OLED_Init/OLED_Set_Dot 迁移至此，保持调用顺序）。
    display_.init();
    display_.setDot(true);
}

void App::updateDisplay()
{
    /* bluetooth_state 固定为 1（与历史行为一致）。 */
    display_.update(static_cast<uint8_t>(data_.battery.level()), 1, data_.progress);
}

void App::averageFiltering(uint16_t *input, uint16_t numChannels, uint16_t totalSamples, uint16_t *out)
{
    const uint16_t samplesPerChannel = totalSamples / numChannels;

    for (uint16_t ch = 0; ch < numChannels; ch++) {
        uint32_t sum = 0;
        for (uint16_t s = 0; s < samplesPerChannel; s++) {
            sum += input[ch + s * numChannels];
        }
        out[ch] = static_cast<uint16_t>(sum / samplesPerChannel);
    }
}

void App::runMeasurementTask()
{
    if (data_.workStatus == WorkState::Calibration) {
        data_.measurement.calibrate(data_.adcInt, kDetectionDeltaTime);
        if (data_.measurement.flag() == ConversionFlag::InProgress) {
            setUvLevel(data_.measurement.value().uvLightLevel);
        }
    } else if (data_.workStatus == WorkState::Working) {
        data_.measurement.update(data_.adcInt, kDetectionDeltaTime);
    } else if (data_.workStatus == WorkState::Save) {
        PersistentStore::save(data_.measurement.value(), data_.measurement.flag());
        data_.workStatus = WorkState::Ready;
    } else if (data_.workStatus == WorkState::Init) {
        // 无操作
    }

    if (data_.workStatus == WorkState::Calibration || data_.workStatus == WorkState::Working) {
        data_.progress = static_cast<uint8_t>(data_.measurement.detectionTime());
    } else if (data_.battery.status() != BattStatus::Normal) {
        data_.progress = static_cast<uint8_t>(data_.battery.adc() / kBatteryVMax * 100);
    }
}

void App::updateWorkStatus()
{
    /* ==================== 1. 意图状态转换 ==================== */
    switch (data_.workStatus) {
        case WorkState::Ready:
            if (data_.hopeStatus == WorkState::Working && data_.measurement.flag() != ConversionFlag::Finished) {
                data_.measurement.reset();
                setUvLevel(data_.measurement.value().uvLightLevel);
                dcCtrlOn();
                data_.workStatus = WorkState::Working;
            } else if (data_.hopeStatus == WorkState::Calibration && data_.measurement.flag() != ConversionFlag::Finished) {
                data_.measurement.reset();
                setUvLevel(data_.measurement.value().uvLightLevel);
                dcCtrlOn();
                data_.workStatus = WorkState::Calibration;
            } else if (data_.hopeStatus == WorkState::Save) {
                data_.workStatus = WorkState::Save;
            }
            break;

        case WorkState::Working:
        case WorkState::Calibration:
        case WorkState::Init:
            if (data_.hopeStatus == WorkState::Ready && data_.measurement.flag() == ConversionFlag::Finished) {
                data_.result = data_.measurement.result();
                data_.workStatus = WorkState::Ready;
            } else if (data_.hopeStatus != WorkState::Ready && data_.measurement.flag() == ConversionFlag::Finished) {
                setUvLevel(kUvCloseLevel);
                dcCtrlOff();
            } else if (data_.hopeStatus == WorkState::Ready && data_.measurement.flag() == ConversionFlag::InProgress) {
                setUvLevel(kUvCloseLevel);
                dcCtrlOff();
                data_.workStatus = WorkState::Ready;
            } else if (data_.hopeStatus == WorkState::Ready && data_.measurement.flag() == ConversionFlag::Ready) {
                setUvLevel(kUvCloseLevel);
                dcCtrlOff();
                data_.workStatus = WorkState::Ready;
            }
            if (data_.input.tilt.status() == 0)
                data_.workStatus = WorkState::ErrTilt;
            else if (data_.input.sw.status() == 0)
                data_.workStatus = WorkState::ErrOpen;
            else if (data_.input.key.status() == 0)
                data_.workStatus = WorkState::ErrNoContainer;
            break;

        case WorkState::ErrTilt:
        case WorkState::ErrOpen:
        case WorkState::ErrLowPower:
        case WorkState::ErrNoContainer:
            if (data_.hopeStatus == WorkState::Ready) {
                data_.workStatus = WorkState::Ready;
            }
            break;

        default:
            break;
    }

    /* ==================== 3. 低电量检测（历史代码已注释，见 docs/DEAD_CODE.md） ==================== */

    /* ==================== 4. 错误恢复 ==================== */
    if (data_.workStatus == WorkState::ErrLowPower &&
        (data_.battery.level() > 2 || data_.battery.status() == BattStatus::Standby)) {
        data_.workStatus = WorkState::Ready;
    }
    if (data_.workStatus == WorkState::ErrOpen && data_.input.sw.status() != 0) {
        data_.workStatus = WorkState::Ready;
    }
    if (data_.workStatus == WorkState::ErrTilt && data_.input.tilt.status() != 0) {
        data_.workStatus = WorkState::Ready;
    }
    if (data_.workStatus == WorkState::ErrNoContainer && data_.input.key.status() != 0) {
        data_.workStatus = WorkState::Ready;
    }

    /* ==================== 5. 电池状态更新 ==================== */
    data_.battery.updateStatus();
}

void App::onAdcComplete()
{
    averageFiltering(data_.adcValue, kAdcChannelCount, kAdcSampleCount, data_.adcAvg);
    updateWorkStatus();
    data_.adcInt = data_.adcAvg[kAdcIntChannel];
    data_.battery.setAdc(data_.adcAvg[kAdcBatChannel]);
    runMeasurementTask();

    if (data_.workStatus == WorkState::Working || data_.workStatus == WorkState::Calibration) {
        data_.input.sw.update(static_cast<uint8_t>(HAL_GPIO_ReadPin(SW_GPIO_Port, SW_Pin)));
        data_.input.tilt.update(static_cast<uint8_t>(HAL_GPIO_ReadPin(TILT_GPIO_Port, TILT_Pin)));
        data_.input.key.update(static_cast<uint8_t>(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin)));
    }
}

} // namespace app

/* 电量等级换算：保留原外部 C 符号（当前无调用点，见 docs/DEAD_CODE.md）。 */
extern "C" int battery_level(float voltage)
{
    return app::BatteryMonitor::levelFor(voltage);
}

extern "C" void App_Init(void)
{
    app::g_app.init();
}

extern "C" void App_UpdateDisplay(void)
{
    app::g_app.updateDisplay();
}

/* HAL 回调入口：与原先在 main.c 中的函数名/签名一致（C 链接）。 */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    app::g_app.onAdcComplete();
}
