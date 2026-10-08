/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "Concentration_Conversion.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
void Average_filtering(uint16_t *input_data, uint16_t num_channels, uint16_t total_samples, uint16_t *output_avg);
void DMA1_Channel1_IRQHandler(void);
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TILT_Pin GPIO_PIN_2
#define TILT_GPIO_Port GPIOA
#define KEY_Pin GPIO_PIN_7
#define KEY_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_12
#define LED_GPIO_Port GPIOB
#define SW_Pin GPIO_PIN_8
#define SW_GPIO_Port GPIOA
#define SHDBY_Pin GPIO_PIN_4
#define SHDBY_GPIO_Port GPIOB
#define CHRG_Pin GPIO_PIN_5
#define CHRG_GPIO_Port GPIOB
#define DC_ctrl_Pin GPIO_PIN_8
#define DC_ctrl_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

#define ADC_BAT_CHANNEL                     1
#define ADC_INT_CHANNEL                     0
#define set_UVlight_level(my_data, level)   (my_data->Concentration_Conversion.Conversion_value.UVlight_level = (level))
#define set_light_level(my_data, level)     (my_data.Concentration_Conversion.Conversion_value.UVlight_level = (level))
#define UVlight_level_update(UVlight_level) __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 4 * UVlight_level)
#define UVlight_ON()                        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4)
#define UVlight_OFF()                       HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_4)
#define DC_ctrl_ON()                        HAL_GPIO_WritePin(DC_ctrl_GPIO_Port, DC_ctrl_Pin, GPIO_PIN_RESET)
#define DC_ctrl_OFF()                       HAL_GPIO_WritePin(DC_ctrl_GPIO_Port, DC_ctrl_Pin, GPIO_PIN_SET)

#define DETECTION_TIME                      10.0f
#define CLOSE_level                         0

#define SW_STATUS                           0 // 0: SW ERR
#define TILT_STATUS                         0 // 0: TILT ERR
#define KEY_STATUS                          0 // 0: KEY ERR

#define V_MAX                               4.2f
#define V_MIN                               3.0f

#define M_MIN(a, b)                         ((a) < (b) ? (a) : (b))
#define M_MAX(a, b)                         ((a) > (b) ? (a) : (b))

#define M_ABS(x)                            ((x) < 0 ? -(x) : (x))

#define M_CLAMP(x, min, max)                (M_MIN((max), M_MAX((min), (x))))


#define RX_BUFFER_SIZE 256

typedef enum {
    NORMAL,
    SHDBY,
    CHRG
} BAT_status_e;
#ifdef __cplusplus
/* C++：作用域枚举（enum class），避免与 Conversion_flag_e::ready 重名；
 * 固定底层类型 uint8_t 以保持 my_data_t 布局不变（ARM EABI 下 work_status_e 原为 1 字节）。 */
enum class work_status_e : uint8_t {
    INIT,
    SAVE,
    calibration,
    working,
    ready,
    err_TILT,
    err_open,
    err_low_pow,
    err_no_cap
};
#else
/* C 侧不使用该枚举的取值，仅需类型占位即可（业务逻辑位于 C++ 的 App 模块）。 */
typedef uint8_t work_status_e;
#endif
typedef struct
{
    float adc_BAT;
    int battery_level;
    BAT_status_e BAT_status;
} BAT_t;

typedef struct {
    uint8_t SW_status;
    uint8_t SW_times;
    uint8_t TILT_status;
    uint8_t TILT_times;
    uint8_t KEY_status;
    uint8_t KEY_times;
} hardware_status_t;

typedef struct
{
    uint16_t ADC_value[20];
    uint16_t ADC_avg[2];
    BAT_t BAT;
    float ADC_INT;
    work_status_e work_status;
    // work_status_e last_status;
    work_status_e hope_status;
    float result;
    uint8_t progress;
    hardware_status_t *hardware_status;
    Concentration_Conversion_t Concentration_Conversion;
    uint64_t time;

} my_data_t;

void Concentration_Conversion_task(my_data_t *my_data);
void Concentration_Conversion_task_init(my_data_t *my_data);
int battery_level(float voltage);
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
