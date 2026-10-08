/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Concentration_Conversion.h"
#include "OLED.h"

#include "FIFO_LOCKFREE.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t I = 0;
my_data_t my_data;
hardware_status_t hardware_status;
// uint32_t ndtr;
#define MSG_SIZE 32
static fifo_data_t msg_buf[MSG_SIZE];
fifo_lockfree_t msg_fifo = FIFO_LOCKFREE_INIT(msg_fifo, msg_buf, MSG_SIZE);

uint8_t rx_dma_buffer[RX_BUFFER_SIZE]; // DMA搬运数据的缓冲区[reference:21]
volatile uint8_t rx_frame_ready = 0;   // 帧接收完成标志
uint8_t rx_work_buffer[RX_BUFFER_SIZE];// 用于处理的工作缓冲区
uint16_t rx_frame_len = 0;             // 当前帧长度
//  extern I2C_HandleTypeDef hi2c1;  // 已初始化的 I2C 句柄

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
    Concentration_Conversion_task_init(&my_data);
    OLED_Init();
    OLED_Set_Dot(1); // 显示右侧圆点

    // HAL_ADC_Start_DMA(&hadc1, (uint32_t *)my_data.ADC_value[0], 20);
    // // HAL_DMA_Start_IT(&hdma_adc1, (uint32_t)&hadc1.Instance->DR, (uint32_t)my_data.ADC_value[0], 20);
    // HAL_TIM_Base_Start_IT(&htim2);
    // HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        OLED_Update(my_data.BAT.battery_level, 1, my_data.progress);
        HAL_Delay(113);
        
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void Average_filtering(uint16_t *input_data, uint16_t num_channels, uint16_t total_samples, uint16_t *output_avg)
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
// void Concentration_Conversion_task(my_data_t *my_data){
//     if()
// }

void Concentration_Conversion_task_init(my_data_t *my_data)
{
    // Concentration_Conversion_init(&my_data->Concentration_Conversion);
    // set_UVlight_level(my_data, 0);

    // 1. 启动DMA接收，让数据在后台自动搬运到 rx_dma_buffer[reference:22]
    HAL_UART_Receive_DMA(&huart1, rx_dma_buffer, RX_BUFFER_SIZE);

    // 2. 开启USART1的空闲中断[reference:23]
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    UVlight_level_update(CLOSE_level);
    DC_ctrl_OFF();
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&my_data->ADC_value[0], 20);
    HAL_Delay(10);

    Concentration_Conversion_init(&my_data->Concentration_Conversion, ms, 100.0f);
    my_data->hardware_status = &hardware_status;
    my_data->hardware_status->SW_status = 1-SW_STATUS;
    my_data->hardware_status->TILT_status = 1-TILT_STATUS;
    my_data->hardware_status->KEY_status = 1-KEY_STATUS;
}
void Concentration_Conversion_task(my_data_t *my_data)
{
    if (my_data->work_status == calibration)
    {
        Concentration_Conversion_calibration(&my_data->Concentration_Conversion, my_data->ADC_INT, DETECTION_TIME);
        if (my_data->Concentration_Conversion.Conversion_flag == not_finish)
        {
            UVlight_level_update(my_data->Concentration_Conversion.Conversion_value.UVlight_level);
        }
    }
    else if (my_data->work_status == working)
    {
        Concentration_Conversion_updata(&my_data->Concentration_Conversion, my_data->ADC_INT, DETECTION_TIME);
    }
    else if (my_data->work_status == SAVE)
    {
        Write_Conversion_Value(&my_data->Concentration_Conversion.Conversion_value,&my_data->Concentration_Conversion);
        my_data->work_status = readay;
    }
    else if (my_data->work_status == INIT)
    {
        ;
    }
    if (my_data->work_status == calibration || my_data->work_status == working)
    {
        my_data->progress = (uint8_t)(my_data->Concentration_Conversion.detection_time);

    }
    else if (my_data->BAT.BAT_status != NORMAL)
    {
        my_data->progress = (uint8_t)(my_data->BAT.adc_BAT / V_MAX * 100);
    }
}
void CC_set_work_status(my_data_t *my_data)
{
    /* ==================== 1. 意图状态转换 ==================== */
    switch (my_data->work_status)
    {
    case readay:
        if (my_data->hope_status == working && my_data->Concentration_Conversion.Conversion_flag != finish)
        {
            Concentration_Conversion_Reset(&my_data->Concentration_Conversion);
            UVlight_level_update(my_data->Concentration_Conversion.Conversion_value.UVlight_level);
            DC_ctrl_ON();
            my_data->work_status = working;
        }
        else if (my_data->hope_status == calibration && my_data->Concentration_Conversion.Conversion_flag != finish)
        {
            Concentration_Conversion_Reset(&my_data->Concentration_Conversion);
            UVlight_level_update(my_data->Concentration_Conversion.Conversion_value.UVlight_level);
            DC_ctrl_ON();
            my_data->work_status = calibration;
        }
        else if (my_data->hope_status == SAVE)
        {
            my_data->work_status = SAVE;
        }
        break;

    case working:
    case calibration:
    case INIT:
        if (my_data->hope_status == readay && my_data->Concentration_Conversion.Conversion_flag == finish)
        {
            my_data->result = get_Result(&my_data->Concentration_Conversion);
            my_data->work_status = readay;
        }
        else if (my_data->hope_status != readay && my_data->Concentration_Conversion.Conversion_flag == finish)
        {
            UVlight_level_update(CLOSE_level);
            DC_ctrl_OFF();
        }
        else if (my_data->hope_status == readay && my_data->Concentration_Conversion.Conversion_flag == not_finish)
        {
            UVlight_level_update(CLOSE_level);
            DC_ctrl_OFF();
            my_data->work_status = readay;
        }
        else if (my_data->hope_status == readay && my_data->Concentration_Conversion.Conversion_flag == ready)
        {
            UVlight_level_update(CLOSE_level);
            DC_ctrl_OFF();
            my_data->work_status = readay;
        }
        if (my_data->hardware_status->TILT_status == TILT_STATUS)
            my_data->work_status = err_TILT;
        else if (my_data->hardware_status->SW_status == SW_STATUS)
            my_data->work_status = err_open;
        else if (my_data->hardware_status->KEY_status == KEY_STATUS)
            my_data->work_status = err_no_cap;
        break;

    case err_TILT:
    case err_open:
    case err_low_pow:
    case err_no_cap:
        if (my_data->hope_status == readay)
        {
            my_data->work_status = readay;
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
    if (my_data->work_status == err_low_pow &&
        (my_data->BAT.battery_level > 2 || my_data->BAT.BAT_status == SHDBY))
    {
        my_data->work_status = readay;
    }
    if (my_data->work_status == err_open &&
        my_data->hardware_status->SW_status != SW_STATUS)
    {
        my_data->work_status = readay;
    }
    if (my_data->work_status == err_TILT &&
        my_data->hardware_status->TILT_status != TILT_STATUS)
    {
        my_data->work_status = readay;
    }
    if (my_data->work_status == err_no_cap &&
        my_data->hardware_status->KEY_status != KEY_STATUS)
    {
        my_data->work_status = readay;
    }

    /* ==================== 5. 电池状态更新 ==================== */
    if (HAL_GPIO_ReadPin(SHDBY_GPIO_Port, SHDBY_Pin) == GPIO_PIN_RESET)
    {
        my_data->BAT.BAT_status = SHDBY;
    }
    else if (HAL_GPIO_ReadPin(CHRG_GPIO_Port, CHRG_Pin) == GPIO_PIN_RESET)
    {
        my_data->BAT.BAT_status = CHRG;
    }
    else
    {
        my_data->BAT.BAT_status = NORMAL;
    }
}
int battery_level(float voltage)
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

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    Average_filtering(my_data.ADC_value, 2, 20, my_data.ADC_avg);
    CC_set_work_status(&my_data);
    my_data.ADC_INT = my_data.ADC_avg[ADC_INT_CHANNEL];
    my_data.BAT.adc_BAT = my_data.ADC_avg[ADC_BAT_CHANNEL];
    Concentration_Conversion_task(&my_data);
    if (my_data.work_status == working || my_data.work_status == calibration)
    {
        if (HAL_GPIO_ReadPin(SW_GPIO_Port, SW_Pin) != my_data.hardware_status->SW_status)
        {
            my_data.hardware_status->SW_times++;
        }
        if (HAL_GPIO_ReadPin(TILT_GPIO_Port, TILT_Pin) != my_data.hardware_status->TILT_status)
        {
            my_data.hardware_status->TILT_times++;
        }
        if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) != my_data.hardware_status->KEY_status)
        {
            my_data.hardware_status->KEY_times++;
        }
        if (my_data.hardware_status->SW_times > 5)
        {
            my_data.hardware_status->SW_times = 0;
            my_data.hardware_status->SW_status = 1 - my_data.hardware_status->SW_status;
        }
        if (my_data.hardware_status->TILT_times > 5)
        {
            my_data.hardware_status->TILT_times = 0;
            my_data.hardware_status->TILT_status = 1 - my_data.hardware_status->TILT_status;
        }
        if (my_data.hardware_status->KEY_times > 5)
        {
            my_data.hardware_status->KEY_times = 0;
            my_data.hardware_status->KEY_status = 1 - my_data.hardware_status->KEY_status;
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
