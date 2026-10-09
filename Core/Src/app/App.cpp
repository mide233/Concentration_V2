/**
 ******************************************************************************
 * @file    App.cpp
 * @brief   应用层逻辑（C++）：状态机、ADC 完成回调、电量与进度计算。
 *
 ******************************************************************************
 */

#include "App.h"

#include "adc.h"
#include "main.h"
#include "tim.h"

#include "app/AppConfig.hpp"
#include "app/AppState.hpp"
#include "app/Bluetooth.hpp"
#include "app/Display.hpp"
#include "app/Hardware.hpp"
#include "app/UartReceiver.hpp"

namespace app {
namespace {
/*
 * 应用运行状态与业务逻辑。
 * 原先散落在 main.c 的全局 my_data / hardware_status 及其处理函数，
 * 现统一封装为该类的成员与方法，经 g_app 单例访问。
 */
class App {
public:
    void init() {
        // 启动 UART DMA 接收与空闲中断。
        g_uartReceiver.start();
        // 蓝牙模块：复位链路状态、按芯片 UID 生成设备名并进入 AT 配置流程。
        g_bluetooth.init();

        setUvLevel(kUvCloseLevel);
        dcCtrlOff();
        HAL_TIM_Base_Start_IT(&htim2);
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

        HAL_ADC_Start_DMA(&hadc1, reinterpret_cast<uint32_t*>(&data_.adcValue[0]), kAdcSampleCount);
        HAL_Delay(10);

        data_.measurement.init(TimeUnit::Milliseconds, kOnceDetectionTime);

        // 初始输入状态取反（0 表示错误态，与历史 SW/TILT/KEY_STATUS 一致）。
        data_.input.sw.begin(1);
        data_.input.tilt.begin(1);
        data_.input.key.begin(1);

        // 初始化显示（原 main.c 的 OLED_Init/OLED_Set_Dot 迁移至此，保持调用顺序）。
        display_.init();
    }

    void updateDisplay() {
        display_.update(
            static_cast<uint8_t>(data_.battery.level()), g_bluetooth.connected() ? 1 : 0,
            data_.progress, data_.msg, isError());
    }

    // ADC DMA 完成中断：仅拷贝样本快照并置位
    void onAdcComplete() {
        for (uint16_t i = 0; i < kAdcSampleCount; i++) {
            adcSnapshot_[i] = data_.adcValue[i];
        }
        adcPending_ = true;
    }

    // 主循环：处理挂起的 ADC 样本，并按周期刷新显示
    void poll() {
        /* 处理挂起的 ADC 样本（与历史中断节拍一致：每次 DMA 完成处理一次）。 */
        if (adcPending_) {
            adcPending_ = false;
            processAdc();
        }

        /* 处理 UART 收帧挂起（R8：拷贝移出中断，在此主循环上下文完成）。 */
        g_uartReceiver.poll();

        /* 蓝牙：AT 配置/连接状态机与收发推进。 */
        g_bluetooth.poll();

        /* 回显测试：收到一行数据即原样发回（连接态下生效，验证链路）。 */
        if (g_bluetooth.available()) {
            static char echoBuf[kLineMax];
            if (g_bluetooth.readLine(echoBuf, sizeof(echoBuf)) > 0u) {
                g_bluetooth.sendLine(echoBuf);
            }
        }

        /* 显示按固定周期节流（原主循环 HAL_Delay(113) 的等价物）。 */
        const uint32_t now = HAL_GetTick();
        if (now - lastDisplayTick_ >= kDisplayPeriodMs) {
            lastDisplayTick_ = now;
            updateDisplay();
        }
    }

private:
    void averageFiltering(
        uint16_t* input, uint16_t numChannels, uint16_t totalSamples, uint16_t* out) {
        const uint16_t samplesPerChannel = totalSamples / numChannels;

        for (uint16_t ch = 0; ch < numChannels; ch++) {
            uint32_t sum = 0;
            for (uint16_t s = 0; s < samplesPerChannel; s++) {
                sum += input[ch + s * numChannels];
            }
            out[ch] = static_cast<uint16_t>(sum / samplesPerChannel);
        }
    }

    // 原 onAdcComplete 的业务逻辑（移出中断上下文）
    void processAdc() {
        averageFiltering(adcSnapshot_, kAdcChannelCount, kAdcSampleCount, data_.adcAvg);
        updateWorkStatus();
        data_.adcInt = data_.adcAvg[kAdcIntChannel];
        data_.battery.setAdc(data_.adcAvg[kAdcBatChannel]);
        runMeasurementTask();

        if (data_.workStatus == WorkState::Working || data_.workStatus == WorkState::Calibration) {
            data_.input.sw.update(static_cast<uint8_t>(HAL_GPIO_ReadPin(SW_GPIO_Port, SW_Pin)));
            data_.input.tilt.update(
                static_cast<uint8_t>(HAL_GPIO_ReadPin(TILT_GPIO_Port, TILT_Pin)));
            data_.input.key.update(static_cast<uint8_t>(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin)));
        }
    }

    bool isError() {
        return (
            data_.workStatus == WorkState::ErrLowPower || data_.workStatus == WorkState::ErrTilt
            || data_.workStatus == WorkState::ErrOpen
            || data_.workStatus == WorkState::ErrNoContainer);
    }

    void runMeasurementTask() {
        if (data_.workStatus == WorkState::Calibration) {
            data_.measurement.calibrate(data_.adcInt, kDetectionDeltaTime);
            if (data_.measurement.flag() == ConversionFlag::InProgress) {
                setUvLevel(data_.measurement.value().uvLightLevel);
            }
        } else if (data_.workStatus == WorkState::Working) {
            data_.measurement.update(data_.adcInt, kDetectionDeltaTime);
        }

        if (data_.workStatus == WorkState::Calibration || data_.workStatus == WorkState::Working) {
            data_.progress = static_cast<uint8_t>(data_.measurement.detectionTime());
            if (data_.workStatus == WorkState::Calibration)
                data_.msg = const_cast<char*>("WORK");
            else
                data_.msg = const_cast<char*>("CALI");
        } else if (!isError()) {
            data_.progress = static_cast<uint8_t>(data_.battery.adc() / kBatteryVMax * 100);

            if (data_.battery.status() == BattStatus::Charging)
                data_.msg = const_cast<char*>("CHARGE");
            else
                data_.msg = const_cast<char*>("BATTERY");
        }
    }

    void updateWorkStatus() {
        switch (data_.workStatus) {
        case WorkState::Ready:
            if (data_.hopeStatus == WorkState::Working
                && data_.measurement.flag() != ConversionFlag::Finished) {
                data_.measurement.reset();
                setUvLevel(data_.measurement.value().uvLightLevel);
                dcCtrlOn();
                data_.workStatus = WorkState::Working;
            } else if (
                data_.hopeStatus == WorkState::Calibration
                && data_.measurement.flag() != ConversionFlag::Finished) {
                data_.measurement.reset();
                setUvLevel(data_.measurement.value().uvLightLevel);
                dcCtrlOn();
                data_.workStatus = WorkState::Calibration;
            }
            break;

        case WorkState::Working:
        case WorkState::Calibration:
        case WorkState::ErrTilt:
        case WorkState::ErrOpen:
        case WorkState::ErrLowPower:

        case WorkState::ErrNoContainer:
            if (data_.hopeStatus == WorkState::Ready) {
                data_.workStatus = WorkState::Ready;
            }
            break;

        default: break;
        }

        bool is_err_tilt = data_.input.tilt.status() == 0;
        bool is_err_open = data_.input.sw.status() == 0;
        bool is_err_nocontainer = data_.input.key.status() == 0;
        bool is_err_lowpower =
            !(data_.battery.level() > 2 || data_.battery.status() == BattStatus::Standby);

        if (data_.workStatus == WorkState::ErrLowPower && !is_err_lowpower) {
            data_.workStatus = WorkState::Ready;
        }
        if (data_.workStatus == WorkState::ErrOpen && !is_err_open) {
            data_.workStatus = WorkState::Ready;
        }
        if (data_.workStatus == WorkState::ErrTilt && !is_err_tilt) {
            data_.workStatus = WorkState::Ready;
        }
        if (data_.workStatus == WorkState::ErrNoContainer && !is_err_nocontainer) {
            data_.workStatus = WorkState::Ready;
        }

        if (is_err_lowpower && false) {          // Debug: 屏蔽低电量错误，便于调试
            data_.workStatus = WorkState::ErrLowPower;
            data_.msg = const_cast<char*>("LOW POW");
        } else if (is_err_tilt) {
            data_.workStatus = WorkState::ErrTilt;
            data_.msg = const_cast<char*>("TILTING");
        } else if (is_err_open) {
            data_.workStatus = WorkState::ErrOpen;
            data_.msg = const_cast<char*>("OPEN LID");
        } else if (is_err_nocontainer) {
            data_.workStatus = WorkState::ErrNoContainer;
            data_.msg = const_cast<char*>("NO CONT");
        }

        data_.battery.updateStatus();
    }

    AppData data_;
    Display display_;
    volatile bool adcPending_ = false;           // ISR 置位 / 主循环清零
    uint16_t adcSnapshot_[kAdcSampleCount] = {}; // ISR 中的样本快照
    uint32_t lastDisplayTick_ = 0;               // 上次显示刷新时刻（ms）
};

} // namespace

// 文件内唯一实例。
App g_app;

} // namespace app

extern "C" void App_Init(void) { app::g_app.init(); }

extern "C" void App_UpdateDisplay(void) { app::g_app.updateDisplay(); }

extern "C" void App_Poll(void) { app::g_app.poll(); }

/* HAL 回调入口 */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) { app::g_app.onAdcComplete(); }

/* C 接缝：供 stm32f1xx_it.c 调用 */
extern "C" void UartReceiver_HandleIdle(void) { app::g_uartReceiver.handleIdleInterrupt(); }

/* HAL UART 发送完成回调：驱动蓝牙发送队列。 */
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef*) { app::g_bluetooth.onTxComplete(); }
