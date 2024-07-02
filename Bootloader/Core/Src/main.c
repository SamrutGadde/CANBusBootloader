/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f407x_flash.h"
#include "canbus_bootloader.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// is a multiple of CAN_DATA_BYTES for ease
#define FLASH_BUF_MULT 256
#define FLASH_BUF_SIZE CAN_DATA_BYTES *FLASH_BUF_MULT
#define min(a, b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
typedef void (*ptrF)(uint32_t dlyticks);
typedef void (*pFunction)(void);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
enum FLASH_STATUS
{
  NOT_STARTED,
  IN_PROGRESS,
  FINISHED
};

CAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[8];
uint32_t TxMailbox;
CAN_RxHeaderTypeDef RxHeader;
uint8_t RxData[8];
enum FLASH_STATUS status = NOT_STARTED;
uint8_t flashBuf[FLASH_BUF_SIZE];
uint64_t fileLength;
int indexCheck;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void goToApp(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Retargets the C library printf function to the USART.
 * @param  None
 * @retval None
 */
int __io_putchar(int ch)
{
  // Write character to ITM ch.0
  ITM_SendChar(ch);
  return (ch);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan == &hcan1)
  {
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
    {
      Error_Handler();
    }
    printf("CAN RX: ", RxHeader.StdId);
    for (int i = 0; i < 8; i++)
    {
      printf("%02X ", RxData[i]);
    }
    printf("\n");

    // if id == 0x123, we are receiving firmware
    if (RxHeader.StdId == 0x123)
    {

      // first reception, tells length of file
      if (status == NOT_STARTED)
      {
        status = IN_PROGRESS;
        indexCheck = 0;
        fileLength = 0;
        for (int i = 0; i < 8; i++)
        {
          fileLength <<= 8;
          fileLength += RxData[i];
        }
        printf("Receiving file with length %lu\n", fileLength);

        // wipe flash mem before writing
        TxHeader.DLC = 1;
        TxData[0] = status;
        HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
        return;
      }

      uint32_t index = 0;
      uint8_t *data = RxData + CAN_IDX_BYTES;
      for (int i = 0; i < CAN_IDX_BYTES; i++)
      {
        index <<= 8;
        index += RxData[i];
      }

      //	printf("index diff: %d\n", index - indexCheck);
      if (index - indexCheck != 1)
      {
        printf("lost packet?\n");
      }
      indexCheck = index;

      uint8_t bytesToWrite = fileLength < CAN_DATA_BYTES ? fileLength : CAN_DATA_BYTES;
      //	uint8_t bytesToWrite = 6;

      uint32_t writeIdx = CAN_DATA_BYTES * (index % FLASH_BUF_MULT);

      // add data to flash buffer
      memcpy(&flashBuf[writeIdx], data, bytesToWrite);

      // if next index
      if (index > 0 && (index % FLASH_BUF_MULT == FLASH_BUF_MULT - 1 ||
                        bytesToWrite < CAN_DATA_BYTES))
      {
        printf("writing to flash at index %lu\n", index);
        // we need to write flash buffer to flash memory
        uint32_t numWords = (FLASH_BUF_SIZE / 4) + ((FLASH_BUF_SIZE % 4) != 0);
        uint32_t offset = index - (FLASH_BUF_MULT - 1);
        flashWriteApp(offset, (uint32_t *)flashBuf, numWords);
      }

      fileLength -= bytesToWrite;

      if (fileLength <= 0)
      {
        printf("Flashing Finished.\n");
        status = FINISHED;
      }
    }
  }
}

void goToApp(void)
{
  uint32_t JumpAddress;
  pFunction Jump_to_Application;
  printf("Jumping to Application \n");

  if (((*(uint32_t *)FLASH_APP_ADDR) & 0x2FFC0000) == 0x20000000)
  {
    HAL_Delay(100);
    printf("Valid Stack Pointer...\n");

    JumpAddress = *(uint32_t *)(FLASH_APP_ADDR + 4);
    Jump_to_Application = (pFunction)JumpAddress;

    __set_MSP(*(uint32_t *)FLASH_APP_ADDR);
    Jump_to_Application();
  }
  else
  {
    printf("Failed to Start Application\n");
  }
}

void Convert_To_Str(uint32_t *Data, char *Buf)
{
  int numberofbytes = ((strlen((char *)Data) / 4) + ((strlen((char *)Data) % 4) != 0)) * 4;

  for (int i = 0; i < numberofbytes; i++)
  {
    Buf[i] = Data[i / 4] >> (8 * (i % 4));
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
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.StdId = 0x111;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.DLC = 2;
  TxData[0] = 'h';
  TxData[1] = 'i';
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
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  CAN_FilterTypeDef sFilterConfig;

  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0; // set fifo assignment
  sFilterConfig.FilterIdHigh = 0;
  sFilterConfig.FilterIdLow = 0;
  sFilterConfig.FilterMaskIdHigh = 0;
  sFilterConfig.FilterMaskIdLow = 0;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT; // set filter scale
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.FilterBank = 5;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.SlaveStartFilterBank = 7;
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    Error_Handler();
  }

  //  char *data = "hello i am writing this to flash.";
  //  char string[100];
  //  uint32_t rxData[30];
  //
  //  flashEraseAppSectors();
  //  int numofwords = (strlen(data)/4)+((strlen(data)%4)!=0);
  //  uint32_t timeStart = HAL_GetTick();
  //  flashWriteApp(0, (uint32_t*)data, numofwords);
  //  uint32_t timeEnd = HAL_GetTick();
  //
  //  printf("Took %ld ms to write\n", timeEnd - timeStart);
  //
  //  flashReadData(FLASH_APP_ADDR, rxData, numofwords);
  //  Convert_To_Str(rxData, string);
  //
  //  printf("Flash string: %s\n", string);
  flashEraseAppSectors();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    //    if (status == FINISHED) {
    //	status = NOT_STARTED;
    //	goToApp();
    //    }
    //    goToApp();
    //    printf("hello world!\n\r");
    //    HAL_Delay(1000);
    if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK)
    {
      Error_Handler();
    }
    HAL_Delay(1000);
    //
    //     HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    //     HAL_Delay(100);
    //     HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
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
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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
 * @brief CAN1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 3;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_Pin | LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_Pin LED2_Pin */
  GPIO_InitStruct.Pin = LED_Pin | LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
    HAL_Delay(100);
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
