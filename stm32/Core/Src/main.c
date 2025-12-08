/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TRIG_PIN GPIO_PIN_7
#define ECHO_PIN GPIO_PIN_8
#define LED_PIN GPIO_PIN_5

#define DT_PIN GPIO_PIN_8
#define DT_PORT GPIOB
#define SCK_PIN GPIO_PIN_9
#define SCK_PORT GPIOB

#define SENSOR_GPIO_PORT    GPIOA
#define SENSOR_GPIO_PIN     GPIO_PIN_4
#define ADC_CHANNEL_TO_READ ADC_CHANNEL_4

#define ADC_MIN_LEVEL 3000
#define ADC_MAX_LEVEL 4000

#define SERVO_OPEN_ANGLE 180
#define SERVO_CLOSE_ANGLE 0
uint8_t isOpen = 0;
uint32_t openTime;

extern TIM_HandleTypeDef htim1;  // TIM1 handle created by CubeMX
extern TIM_HandleTypeDef htim2;  // TIM1 handle created by CubeMX


// Map angle (0–180) to pulse width (1000–2000 us)
void Servo_Write(uint8_t angle)
{
    if (angle > 180) angle = 180;

    uint16_t pulse = 1000 + (angle * 1000 / 180);  // 1000–2000 us
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse);
}

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint32_t duration = 0;
uint32_t distance = 0;
char uartBuf[50];

float calibration_factor = 1098.3f;

int32_t tare = 0;
float knownOriginal = 1;
float knownHX711 = 1;
int weight;
int percent_level;
uint8_t rx_byte;
uint8_t feed_trigger;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void delay_us(uint16_t us){
	__HAL_TIM_SET_COUNTER(&htim1, 0);
	while (__HAL_TIM_GET_COUNTER(&htim1) < us);
}

int32_t getHX711(void){
	uint32_t data = 0;
	uint32_t startTime = HAL_GetTick();
	while(HAL_GPIO_ReadPin(DT_PORT,DT_PIN) == GPIO_PIN_SET){
		if(HAL_GetTick() - startTime > 200){
			return 0;
		}
	}
	for(int8_t len = 0; len < 24; len++){
		HAL_GPIO_WritePin(SCK_PORT, SCK_PIN,GPIO_PIN_SET);
		delay_us(1);
		data = data << 1;
		HAL_GPIO_WritePin(SCK_PORT, SCK_PIN,GPIO_PIN_RESET);
		delay_us(1);
		if(HAL_GPIO_ReadPin(DT_PORT, DT_PIN) == GPIO_PIN_SET){
			data++;
		}
	}
	HAL_GPIO_WritePin(SCK_PORT,SCK_PIN,GPIO_PIN_SET);
	delay_us(1);
	HAL_GPIO_WritePin(SCK_PORT,SCK_PIN,GPIO_PIN_RESET);
	delay_us(1);

	if (data & 0x800000) {
	      data |= 0xFF000000;
	  }

	return (int32_t)data;
}

void calibrate_tare(void) {
    int32_t total = 0;
    // Take 10 readings
    for(int i = 0; i < 10; i++) {
        total += getHX711();
        HAL_Delay(10);
    }
    // Set the average as the new "Zero" point
    tare = total / 10;
}

int weigh(){
	int32_t total = 0;
	int32_t samples = 10;
	float coefficient;
	for(uint16_t i =0; i<samples; i++){
		total += getHX711();
	}
	int32_t average = total / samples;
	coefficient = knownOriginal / knownHX711;

	int32_t raw_diff = (int32_t)average - (int32_t)tare;

	if (raw_diff < 0) {
	        return 0;
	}

	float grams = (average - tare) / calibration_factor;

	return (int)grams;
}

int Water_Level(uint16_t value) {
    char tx_buffer[50]; // Increased buffer size for safety
    int len;
    int32_t current_adc = (int32_t)value; // Use signed integer for subtraction safety
    int32_t percent_level;

    // 1. Calculate the total range (P = V_max - V_min)
    const int32_t total_range = ADC_MAX_LEVEL - ADC_MIN_LEVEL;

    // Safety check to prevent division by zero and ensure a valid range
    if (total_range <= 0) {
        // If range is invalid, just report 0% or an error
        percent_level = 0;
    } else {
        // 2. Calculate the difference from the minimum (V_current - V_min)
        int32_t numerator = current_adc - ADC_MIN_LEVEL;

        // 3. Clamp the numerator (ensure value is not below V_min or above V_max)
        if (numerator < 0) {
            numerator = 0; // Below V_min means 0%
        } else if (numerator > total_range) {
            numerator = total_range; // Above V_max means 100%
        }

        // 4. Calculate Percentage: (Numerator / Total_Range) * 100
        // We use integer math for this simple function: (N * 100) / D
        percent_level = (numerator * 100) / total_range;
    }

    // Use snprintf to format the data into a string
    len = snprintf(tx_buffer, sizeof(tx_buffer), "Level: %ld percent\r\n", percent_level);

    // Transmit the string
//    HAL_UART_Transmit(&huart2, (uint8_t*)tx_buffer, len, 1000);
    return (int) percent_level;
}

int loop_adc_read(void) {
    uint32_t total_reading = 0;
    const uint8_t num_samples = 10; // Take 10 samples

    for (int i = 0; i < num_samples; i++) {
        // 1. Start ADC Conversion
        HAL_ADC_Start(&hadc1);

        // 2. Wait for the conversion to complete
        if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
            // 3. Get the digital value
            total_reading += HAL_ADC_GetValue(&hadc1);
        }
        // Small delay between samples is optional but can help
        HAL_Delay(5);
    }

    // Calculate the average
    uint16_t average_reading = (uint16_t)(total_reading / num_samples);

    // 4. Send the calculated percentage value
    int32_t percent_level = Water_Level(average_reading);


    return (int) percent_level;
    // 5. Short delay before the next set of readings
    // The total delay here is already (5ms * 10 samples) + 100ms = 150ms.
    // HAL_Delay(100); // Removing the extra delay as the loop already has a delay.
}

void Run_Manual_Feed(void){
  if(isOpen == 0){
	  Servo_Write(SERVO_OPEN_ANGLE);
	  openTime = HAL_GetTick();
	  char log[50];
	  int len = snprintf(log, sizeof(log),"m\r\n");
	  HAL_UART_Transmit(&huart2, (uint8_t*)log, (uint16_t)len, 1000);
	  HAL_UART_Transmit(&huart1, (uint8_t*)log, (uint16_t)len, 1000);
	  isOpen = 1;
  }
  else{
	  if(HAL_GetTick() - openTime > 200){
		  Servo_Write(SERVO_CLOSE_ANGLE);
		  isOpen = 0;
	  }
  }
}
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
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  Servo_Write(SERVO_CLOSE_ANGLE);

  HAL_GPIO_WritePin(SCK_PORT, SCK_PIN, GPIO_PIN_SET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(SCK_PORT, SCK_PIN, GPIO_PIN_RESET);
  HAL_Delay(10);

  HAL_Delay(500);
  calibrate_tare();
  HAL_UART_Receive_IT(&huart1,&rx_byte,1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_RESET);
	  delay_us(2);
	  HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_SET);
	  delay_us(10);
	  HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_RESET);

//	  while (HAL_GPIO_ReadPin(GPIOA, ECHO_PIN) == GPIO_PIN_RESET);
//
//	  __HAL_TIM_SET_COUNTER(&htim1, 0);
//
//	  while (HAL_GPIO_ReadPin(GPIOA, ECHO_PIN) == GPIO_PIN_SET);
//
//	  duration = __HAL_TIM_GET_COUNTER(&htim1);
//
//	  distance = duration / 58;

	  uint32_t pMillis = HAL_GetTick(); // Get current time
	        uint8_t sensor_ok = 1;            // Flag to track if sensor is working

	        // WAIT FOR ECHO START (Timeout: 5ms)
	        // If the sensor is unplugged, this loop will break after 5ms
	        while (HAL_GPIO_ReadPin(GPIOA, ECHO_PIN) == GPIO_PIN_RESET) {
	            if (HAL_GetTick() - pMillis > 5) {
	                sensor_ok = 0;
	                break;
	            }
	        }

	        if (sensor_ok) {
	            __HAL_TIM_SET_COUNTER(&htim1, 0); // Reset Timer
	            pMillis = HAL_GetTick();          // Reset Timeout Counter

	            // WAIT FOR ECHO END (Timeout: 50ms - max range approx 8m)
	            while (HAL_GPIO_ReadPin(GPIOA, ECHO_PIN) == GPIO_PIN_SET) {
	                if (HAL_GetTick() - pMillis > 50) {
	                    sensor_ok = 0;
	                    break;
	                }
	            }
	        }

	        if (sensor_ok) {
	            duration = __HAL_TIM_GET_COUNTER(&htim1);
	            distance = duration / 58;
	        } else {
	            // If unplugged or error, set distance to 0 (or a safe value like 999)
	            distance = 0;
	        }

//	  if (distance <= 40) {
//		  HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_SET);
//		  sprintf(uartBuf, "Distance: %lu cm\r\n", distance);
//		  HAL_UART_Transmit(&huart2, (uint8_t*)uartBuf, strlen(uartBuf), 100);
//	  } else {
//		  HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_RESET);
//	  }



	  weight = weigh();

	  //if(weight > 5){
//		  char buffer[50];
//
//		  sprintf(buffer, "Weight: %d grams\r\n", weight);
//
//		  HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer),100);

	  //}

	  if((weight > 0 && weight < 20) && distance < 10){
		  if(isOpen == 0){
			  Servo_Write(SERVO_OPEN_ANGLE);
			  openTime = HAL_GetTick();
			  char log[50];
			  int len = snprintf(log, sizeof(log),"a\r\n");
			  HAL_UART_Transmit(&huart2, (uint8_t*)log, (uint16_t)len, 1000);
			  HAL_UART_Transmit(&huart1, (uint8_t*)log, (uint16_t)len, 1000);
			  isOpen = 1;
		  }
		  else{
			  if(HAL_GetTick() - openTime > 200){
				  Servo_Write(SERVO_CLOSE_ANGLE);
				  isOpen = 0;
			  }
		  }
	  }else if(isOpen == 1){
		  Servo_Write(SERVO_CLOSE_ANGLE);
		  isOpen = 0;
	  }

	  percent_level = loop_adc_read();

	  HAL_Delay(1000);
	  char combined_buffer[50];
	  int len = snprintf(combined_buffer, sizeof(combined_buffer),
						 "d:%d,%d,%d\r\n",
						 (int)distance,
						 (int)weight,
						 (int)percent_level);

	  HAL_UART_Transmit(&huart2, (uint8_t*)combined_buffer, (uint16_t)len, 1000);
	  HAL_UART_Transmit(&huart1, (uint8_t*)combined_buffer, (uint16_t)len, 1000);
	  if (feed_trigger == 1) {
		  Run_Manual_Feed(); // Call your function
		  feed_trigger = 0;  // Reset flag
	  }
//	  char test = 'T';
//	  int len = 10;
//	  HAL_UART_Transmit(&huart2, (uint8_t*)&test, (uint16_t)len, 1000);
//	  HAL_UART_Transmit(&huart1, (uint8_t*)&test, (uint16_t)len, 1000);
//	  HAL_Delay(1000);
//	  HAL_UART_Transmit(&huart2, (uint8_t*)distance, len, 1000);


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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA7 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
	void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
	{
	  if (huart->Instance == USART1) // Check if it's the ESP32 UART
	  {
		// Check if the received character is 'f'
		if (rx_byte == 'f') {
			feed_trigger = 1; // Set the flag, don't run motor here (keep interrupts short!)
		}

		// Restart the listening for the next byte
		HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
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
