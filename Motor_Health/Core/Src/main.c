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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ina219.h"
#include "ntc.h"
#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define FAULT_OVERLOAD  (1U << 0)
#define FAULT_STALL     (1U << 1)
#define FAULT_LOW_VOLT  (1U << 2)
#define FAULT_HIGH_TEMP (1U << 3)
#define FAULT_SENSOR    (1U << 4)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

osThreadId sensorTaskHandle;
osThreadId faultTaskHandle;
osThreadId canTaskHandle;
osThreadId motorTaskHandle;
/* USER CODE BEGIN PV */

/* CAN status: 0 = normal, 1 = possible overload */
volatile uint8_t faultFlags = 0;

volatile uint8_t motorDutyPercent = 50;
extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;
HAL_StatusTypeDef status;

	float Voltage;
	float current;
	float Power;

	float temperature;
	float rpm;

	int8_t direction;

		uint8_t data[2];
	  uint16_t config;
/////////////////////////****MAIN****///////////////////////////////////////////////
	  typedef struct {
	      float rpm;
	      float current;
	      float voltage;
	      float temperature;
	  } SensorData_t;

	  QueueHandle_t sensorQueue;
	  QueueHandle_t faultQueue;

	  typedef enum
	  {
	      MOTOR_OFF = 0,
	      MOTOR_STARTING,
	      MOTOR_RUNNING
	  } MotorState_t;

	  volatile MotorState_t motorState = MOTOR_OFF;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM2_Init(void);
static void MX_CAN_Init(void);
void SensorTask(void const * argument);
void FaultTask(void const * argument);
void CANTask(void const * argument);
void MotorTask(void const * argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  MX_CAN_Init();
  /* USER CODE BEGIN 2 */



  status = HAL_I2C_Mem_Read(&hi2c1,
                            (0x40 << 1),
                            0x00,
                            I2C_MEMADD_SIZE_8BIT,
                            data,
                            2,
                            100);

  if (status == HAL_OK)
  {
      config = ((uint16_t)data[0] << 8) | data[1];
  }

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  sensorQueue = xQueueCreate(1, sizeof(SensorData_t));
  faultQueue  = xQueueCreate(1, sizeof(SensorData_t));

  if (sensorQueue == NULL || faultQueue == NULL)
  {
      Error_Handler();
  }
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of sensorTask */
  osThreadDef(sensorTask, SensorTask, osPriorityHigh, 0, 128);
  sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

  /* definition and creation of faultTask */
  osThreadDef(faultTask, FaultTask, osPriorityAboveNormal, 0, 128);
  faultTaskHandle = osThreadCreate(osThread(faultTask), NULL);

  /* definition and creation of canTask */
  osThreadDef(canTask, CANTask, osPriorityNormal, 0, 128);
  canTaskHandle = osThreadCreate(osThread(canTask), NULL);

  /* definition and creation of motorTask */
  osThreadDef(motorTask, MotorTask, osPriorityAboveNormal, 0, 128);
  motorTaskHandle = osThreadCreate(osThread(motorTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65534;
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
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(IN2_GPIO_Port, IN2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : IN2_Pin */
  GPIO_InitStruct.Pin = IN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(IN2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : IN1_Pin */
  GPIO_InitStruct.Pin = IN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(IN1_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_SensorTask */
/**
  * @brief  Function implementing the sensorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_SensorTask */
void SensorTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
	HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
	int16_t lastCount = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
	TickType_t lastSampleTick = xTaskGetTickCount();
	uint8_t verifyCnt = 0;

	INA219_Init(&hi2c1);   // one-time setup

  for(;;)
  {
	  osDelay(100);

	  int16_t currentCount = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
	  TickType_t now = xTaskGetTickCount();
	  TickType_t elapsedTicks = now - lastSampleTick;
	  int16_t delta = (int16_t)(currentCount - lastCount);

	  lastCount = currentCount;
	  lastSampleTick = now;

	  if (elapsedTicks > 0)
	  {
	      float elapsedSeconds = (float)elapsedTicks / (float)configTICK_RATE_HZ;
	      rpm = ((float)delta * 60.0f) / (3120.0f * elapsedSeconds);
	  }

	  if (delta > 0)      direction = 1;
	  else if (delta < 0) direction = -1;
	  else                direction = 0;

	  if (++verifyCnt >= 10) { verifyCnt = 0; INA219_Verify(&hi2c1); }

	  Voltage = INA219_ReadVoltage(&hi2c1);
	  current = INA219_ReadCurrent(&hi2c1);
	  Power   = INA219_ReadPower(&hi2c1);

	  temperature = NTC_ReadTemperature(&hadc1);

	  SensorData_t sensorData = { rpm, current, Voltage, temperature };
	  xQueueOverwrite(sensorQueue, &sensorData);
	  xQueueOverwrite(faultQueue, &sensorData);


  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_FaultTask */
/**
* @brief Function implementing the faultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_FaultTask */
void FaultTask(void const *argument)
{
    SensorData_t sample;

    uint8_t previousDuty = 0;
    TickType_t settlingStarted = 0;
    TickType_t overloadStarted = 0;
    TickType_t recoveryStarted = 0;
    TickType_t stallStarted = 0;
    TickType_t lowVoltStarted = 0;
    TickType_t highTempStarted = 0;
    TickType_t validStarted = 0;

    float filteredCurrent = 0.0f;

    for (;;)
    {
    	if (xQueueReceive(faultQueue, &sample,
    	                  pdMS_TO_TICKS(1000)) != pdTRUE)
    	{
    	    /* SensorTask stopped producing samples. */
    	    faultFlags = FAULT_SENSOR;
    	    validStarted = 0;
    	    overloadStarted = 0;
    	    recoveryStarted = 0;
    	    stallStarted = 0;
    	    lowVoltStarted = 0;
    	    highTempStarted = 0;
    	    filteredCurrent = 0.0f;
    	    continue;
    	}

        uint8_t duty = motorDutyPercent;
        TickType_t now = xTaskGetTickCount();
        uint8_t flags = faultFlags;

        uint8_t invalid =
            !isfinite(sample.rpm) ||
            !isfinite(sample.current) ||
            !isfinite(sample.voltage) ||
            !isfinite(sample.temperature) ||
            sample.voltage < 0.0f ||
            sample.voltage > 9.0f ||
            sample.current < -0.5f ||
            sample.temperature < -20.0f ||
            sample.temperature > 120.0f;

        if (invalid)
        {
            /* One invalid sample immediately makes readings untrusted. */
            faultFlags = FAULT_SENSOR;
            validStarted = 0;
            overloadStarted = 0;
            recoveryStarted = 0;
            stallStarted = 0;
            lowVoltStarted = 0;
            highTempStarted = 0;
            filteredCurrent = 0.0f;
            continue;
        }

        if (flags & FAULT_SENSOR)
        {
            /* Require 1 second of uninterrupted valid readings. */
            if (validStarted == 0)
            {
                validStarted = now;
                continue;
            }

            if ((TickType_t)(now - validStarted) <
                pdMS_TO_TICKS(1000))
            {
                continue;
            }

            faultFlags = 0;
            flags = 0;
            validStarted = 0;

            /* Restart startup settling after sensor recovery. */
            settlingStarted = now;
            motorState = (duty == 0) ? MOTOR_OFF : MOTOR_STARTING;
            previousDuty = duty;
            filteredCurrent = sample.current;

            overloadStarted = 0;
            recoveryStarted = 0;
            stallStarted = 0;
            lowVoltStarted = 0;
            highTempStarted = 0;
            continue;
        }

        validStarted = 0;

        if (duty == 0)
        {
            motorState = MOTOR_OFF;
            faultFlags = 0;
            overloadStarted = 0;
            recoveryStarted = 0;
            stallStarted = 0;
            lowVoltStarted = 0;
            highTempStarted = 0;
            filteredCurrent = 0.0f;
            previousDuty = duty;
            continue;
        }

        if (duty != previousDuty)
        {
            settlingStarted = now;
            motorState = MOTOR_STARTING;
            flags = 0;
            overloadStarted = 0;
            recoveryStarted = 0;
            stallStarted = 0;
            filteredCurrent = sample.current;
        }
        else
        {
            filteredCurrent =
                0.7f * filteredCurrent + 0.3f * sample.current;
        }

        if ((TickType_t)(now - settlingStarted) >=
            pdMS_TO_TICKS(2000))
        {
            motorState = MOTOR_RUNNING;
        }

        /* Provisional 2-cell low-supply-voltage warning. */
        if (sample.voltage < 6.4f)
        {
            if (lowVoltStarted == 0)
                lowVoltStarted = now;
            else if ((TickType_t)(now - lowVoltStarted) >=
                     pdMS_TO_TICKS(2000))
                flags |= FAULT_LOW_VOLT;
        }
        else
        {
            lowVoltStarted = 0;

            if (sample.voltage > 6.6f)
                flags &= (uint8_t)~FAULT_LOW_VOLT;
        }

        /* Provisional casing-temperature warning. */
        if (sample.temperature > 60.0f)
        {
            if (highTempStarted == 0)
                highTempStarted = now;
            else if ((TickType_t)(now - highTempStarted) >=
                     pdMS_TO_TICKS(2000))
                flags |= FAULT_HIGH_TEMP;
        }
        else
        {
            highTempStarted = 0;

            if (sample.temperature < 55.0f)
                flags &= (uint8_t)~FAULT_HIGH_TEMP;
        }

        if (motorState != MOTOR_RUNNING)
        {
            flags &= (uint8_t)~(FAULT_OVERLOAD | FAULT_STALL);
            overloadStarted = 0;
            recoveryStarted = 0;
            stallStarted = 0;
        }
        else
        {
            /* Measured near-stall rule at 50% duty. */
            if (duty == 50 &&
                sample.rpm < 20.0f &&
                filteredCurrent > 0.45f)
            {
                if (stallStarted == 0)
                    stallStarted = now;
                else if ((TickType_t)(now - stallStarted) >=
                         pdMS_TO_TICKS(500))
                    flags |= FAULT_STALL;
            }
            else
            {
                stallStarted = 0;

                if (sample.rpm > 40.0f || duty != 50)
                    flags &= (uint8_t)~FAULT_STALL;
            }

            float tripRpm = 0.0f;
            float tripCurrent = 0.0f;
            float clearRpm = 0.0f;
            float clearCurrent = 0.0f;
            uint8_t supportedDuty = 1;

            if (duty == 50)
            {
                tripRpm = 85.0f;
                tripCurrent = 0.34f;
                clearRpm = 90.0f;
                clearCurrent = 0.32f;
            }
            else if (duty == 75)
            {
                tripRpm = 112.0f;
                tripCurrent = 0.33f;
                clearRpm = 116.0f;
                clearCurrent = 0.32f;
            }
            else
            {
                supportedDuty = 0;
            }

            if (!supportedDuty || (flags & FAULT_STALL))
            {
                flags &= (uint8_t)~FAULT_OVERLOAD;
                overloadStarted = 0;
                recoveryStarted = 0;
            }
            else if (!(flags & FAULT_OVERLOAD))
            {
                if (sample.rpm < tripRpm &&
                    sample.current > tripCurrent)
                {
                    if (overloadStarted == 0)
                        overloadStarted = now;
                    else if ((TickType_t)(now - overloadStarted) >=
                             pdMS_TO_TICKS(1000))
                        flags |= FAULT_OVERLOAD;
                }
                else
                {
                    overloadStarted = 0;
                }
            }
            else
            {
                if (sample.rpm > clearRpm &&
                    sample.current < clearCurrent)
                {
                    if (recoveryStarted == 0)
                        recoveryStarted = now;
                    else if ((TickType_t)(now - recoveryStarted) >=
                             pdMS_TO_TICKS(1000))
                    {
                        flags &= (uint8_t)~FAULT_OVERLOAD;
                        overloadStarted = 0;
                        recoveryStarted = 0;
                    }
                }
                else
                {
                    recoveryStarted = 0;
                }
            }
        }

        faultFlags = flags;
        previousDuty = duty;
    }
}
/* USER CODE BEGIN Header_CANTask */
/**
* @brief Function implementing the canTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CANTask */
void CANTask(void const *argument)
{
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0;
    filter.FilterIdLow = 0;
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow = 0;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
        Error_Handler();

    if (HAL_CAN_Start(&hcan) != HAL_OK)
        Error_Handler();

    CAN_TxHeaderTypeDef sensorHeader = {0};
    sensorHeader.StdId = 0x100;
    sensorHeader.IDE = CAN_ID_STD;
    sensorHeader.RTR = CAN_RTR_DATA;
    sensorHeader.DLC = 8;
    sensorHeader.TransmitGlobalTime = DISABLE;

    CAN_TxHeaderTypeDef statusHeader = {0};
    statusHeader.StdId = 0x101;
    statusHeader.IDE = CAN_ID_STD;
    statusHeader.RTR = CAN_RTR_DATA;
    statusHeader.DLC = 3;
    statusHeader.TransmitGlobalTime = DISABLE;

    uint32_t txMailbox;
    SensorData_t data;
    TickType_t lastStatusTick = 0;

    for (;;)
    {
        /* SensorTask normally supplies a reading about every 100 ms. */
        if (xQueueReceive(sensorQueue, &data,
                          pdMS_TO_TICKS(100)) == pdTRUE)
        {
            /* Do not convert NaN/infinite readings to integer CAN data. */
        	if (isfinite(data.rpm) &&
        	    isfinite(data.current) &&
        	    isfinite(data.voltage) &&
        	    isfinite(data.temperature) &&
        	    data.voltage >= 0.0f &&
        	    data.voltage <= 9.0f)
        	{
                int16_t rpmScaled =
                    (int16_t)(data.rpm * 10.0f);
                int16_t currentScaled =
                    (int16_t)(data.current * 1000.0f);
                int16_t voltageScaled =
                    (int16_t)(data.voltage * 100.0f);
                int16_t tempScaled =
                    (int16_t)(data.temperature * 10.0f);

                uint8_t sensorData[8] = {
                    (uint8_t)((rpmScaled >> 8) & 0xFF),
                    (uint8_t)(rpmScaled & 0xFF),
                    (uint8_t)((currentScaled >> 8) & 0xFF),
                    (uint8_t)(currentScaled & 0xFF),
                    (uint8_t)((voltageScaled >> 8) & 0xFF),
                    (uint8_t)(voltageScaled & 0xFF),
                    (uint8_t)((tempScaled >> 8) & 0xFF),
                    (uint8_t)(tempScaled & 0xFF)
                };

                if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0)
                {
                    HAL_CAN_AddTxMessage(&hcan, &sensorHeader,
                                         sensorData, &txMailbox);
                }
            }
        }

        /* Report fault status even if a sensor reading was invalid. */
        TickType_t now = xTaskGetTickCount();

        if ((TickType_t)(now - lastStatusTick) >=
            pdMS_TO_TICKS(500))
        {
            uint8_t duty = motorDutyPercent;

            uint8_t statusData[3] = {
                faultFlags,
                duty,
                (duty == 50 || duty == 75) ? 1U : 0U
            };

            if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0 &&
                HAL_CAN_AddTxMessage(&hcan, &statusHeader,
                                     statusData, &txMailbox) == HAL_OK)
            {
                lastStatusTick = now;
            }
        }
    }
}

/* USER CODE BEGIN Header_MotorTask */
/**
* @brief Function implementing the motorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_MotorTask */
void MotorTask(void const * argument)
{
  /* USER CODE BEGIN MotorTask */
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

	HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin,GPIO_PIN_SET);
	HAL_GPIO_WritePin(IN2_GPIO_Port, IN2_Pin, GPIO_PIN_RESET);


	uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim2);

  /* Infinite loop */
  for(;;)
  {
	  uint32_t compare =
	      ((uint32_t)motorDutyPercent * (period + 1U)) / 100U;
	  __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,compare);

    osDelay(100);
  }
  /* USER CODE END MotorTask */
}

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

#ifdef  USE_FULL_ASSERT
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
