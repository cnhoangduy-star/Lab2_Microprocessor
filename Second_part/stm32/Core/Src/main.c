/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
/* ===== Chọn bài demo: 9 (chữ "A" đứng yên) hoặc 10 (chữ "A" dịch trái) ===== */
#define EXERCISE          10

#define TIMER_CYCLE       10    // TIM2: 8MHz/(7999+1)/(9+1) = 100Hz -> ngắt mỗi 10ms
#define NUM_TIMERS        6     // số software timer

/* Chỉ số software timer */
#define T_CLOCK           0     // 1000ms: đếm giây + nháy DOT (Ex7)
#define T_SCAN_7SEG       1     // quét 7-seg (Ex8)
#define T_SCAN_MATRIX     2     // quét LED matrix (Ex9)
#define T_SHIFT           3     // dịch chữ (Ex10)
#define T_LED_RED         4     // ví dụ Program 1.8 (Ex6)
#define T_SELFTEST        5     // chế độ dò phân cực matrix

#define SCAN_7SEG_MS      250   // 4 LED x 250ms = 1s -> quét 1Hz như Ex4
#define SCAN_MATRIX_MS    10    // mỗi cột 10ms -> 8 cột = 80ms/khung (đổi thành 200 để xem từng cột sáng lần lượt)
#define SHIFT_MS          200   // Ex10: dịch 1 cột mỗi 200ms

/* ===== CHẾ ĐỘ DÒ PHÂN CỰC MATRIX =====
 * 1: cứ 5 giây đổi 1 tổ hợp phân cực; cả 4 con 7-seg hiện SỐ THỨ TỰ tổ hợp (1..4),
 *    matrix hiện chữ "A" đứng yên. Ghi lại số nào cho ra chữ "A" rõ nhất.
 * 0: chạy bình thường theo cấu hình bên dưới. */
#define MATRIX_SELFTEST   0

/* ===== Cấu hình phân cực LED matrix: chỉnh ở đây nếu hiển thị sai ===== */
#define MATRIX_COL_ON       GPIO_PIN_SET    // ENM mức cao -> ULN2803 dẫn -> kéo chân cột (cathode) xuống thấp -> cột BẬT
#define MATRIX_ROW_ON       GPIO_PIN_SET    // ROW mức cao -> cấp anode -> LED trên hàng đó sáng
#define MATRIX_TRANSPOSE    0               // 1 nếu chữ "A" bị nằm ngang (xoay 90 độ)
#define MATRIX_LINE_REVERSE 0               // 1 nếu chữ "A" bị lộn ngược (đảo thứ tự đường quét ENM)
#define MATRIX_ROW_REVERSE  0               // 1 nếu chữ "A" bị soi gương / chạy sang PHẢI (đảo thứ tự bit dữ liệu)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
/* ---------- Software timer (Ex6) ---------- */
volatile int timer_counter[NUM_TIMERS] = {0};
volatile int timer_flag[NUM_TIMERS]    = {0};

/* ---------- 4 LED 7 đoạn + đồng hồ (Ex3, Ex5, Ex7, Ex8) ---------- */
const int MAX_LED = 4;
int index_led = 0;
int led_buffer[4] = {1, 2, 3, 4};
int hour = 15, minute = 8, second = 50;

static GPIO_TypeDef* const SEG_PORT[7] = {SEG0_GPIO_Port, SEG1_GPIO_Port, SEG2_GPIO_Port,
                                          SEG3_GPIO_Port, SEG4_GPIO_Port, SEG5_GPIO_Port, SEG6_GPIO_Port};
static const uint16_t SEG_PIN[7] = {SEG0_Pin, SEG1_Pin, SEG2_Pin, SEG3_Pin, SEG4_Pin, SEG5_Pin, SEG6_Pin};
static GPIO_TypeDef* const EN_PORT[4] = {EN0_GPIO_Port, EN1_GPIO_Port, EN2_GPIO_Port, EN3_GPIO_Port};
static const uint16_t EN_PIN[4] = {EN0_Pin, EN1_Pin, EN2_Pin, EN3_Pin};

/* Mã 7 đoạn: bit0 = a ... bit6 = g, bit = 1 nghĩa là đoạn sáng */
static const uint8_t SEG_CODE[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

/* ---------- LED matrix 8x8 (Ex9, Ex10) ---------- */
const int MAX_LED_MATRIX = 8;
GPIO_PinState matrix_col_on = MATRIX_COL_ON;   // mức bật đường quét (ENM)
GPIO_PinState matrix_row_on = MATRIX_ROW_ON;   // mức làm LED sáng (ROW)
int selftest_combo = 0;                        // 0..3 -> hiển thị 1..4
int index_led_matrix = 0;

/* matrix_buffer[c] = dữ liệu cột c (trái -> phải); bit r = hàng r (bit0 = hàng trên cùng)
 *
 *   . . . # # . . .
 *   . . # . . # . .
 *   . # . . . . # .
 *   . # . . . . # .
 *   . # # # # # # .
 *   . # . . . . # .
 *   . # . . . . # .
 *   . . . . . . . .
 */
uint8_t matrix_buffer[8] = {0x00, 0x7C, 0x12, 0x11, 0x11, 0x12, 0x7C, 0x00};

/* Ex10: khung hình 16 cột = chữ "A" + 8 cột trống, cửa sổ 8 cột trượt sang trái */
static const uint8_t anim_frame[16] = {0x00, 0x7C, 0x12, 0x11, 0x11, 0x12, 0x7C, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
int anim_offset = 0;

static GPIO_TypeDef* const ENM_PORT[8] = {ENM0_GPIO_Port, ENM1_GPIO_Port, ENM2_GPIO_Port, ENM3_GPIO_Port,
                                          ENM4_GPIO_Port, ENM5_GPIO_Port, ENM6_GPIO_Port, ENM7_GPIO_Port};
static const uint16_t ENM_PIN[8] = {ENM0_Pin, ENM1_Pin, ENM2_Pin, ENM3_Pin,
                                    ENM4_Pin, ENM5_Pin, ENM6_Pin, ENM7_Pin};
static GPIO_TypeDef* const ROW_PORT[8] = {ROW0_GPIO_Port, ROW1_GPIO_Port, ROW2_GPIO_Port, ROW3_GPIO_Port,
                                          ROW4_GPIO_Port, ROW5_GPIO_Port, ROW6_GPIO_Port, ROW7_GPIO_Port};
static const uint16_t ROW_PIN[8] = {ROW0_Pin, ROW1_Pin, ROW2_Pin, ROW3_Pin,
                                    ROW4_Pin, ROW5_Pin, ROW6_Pin, ROW7_Pin};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ================= Software timer (Ex6) ================= */
void setTimer(int id, int duration) {
  timer_counter[id] = duration / TIMER_CYCLE;
  timer_flag[id] = 0;
}

/* Gọi trong ngắt mỗi TIMER_CYCLE ms: chỉ đếm, không xử lý gì nặng */
void timer_run(void) {
  for (int i = 0; i < NUM_TIMERS; i++) {
    if (timer_counter[i] > 0) {
      timer_counter[i]--;
      if (timer_counter[i] == 0) timer_flag[i] = 1;
    }
  }
}

/* ================= 7 đoạn ================= */
/* Common anode: đoạn sáng khi chân SEG ở mức THẤP */
void display7SEG(int num) {
  if (num < 0 || num > 9) num = 0;
  uint8_t code = SEG_CODE[num];
  for (int i = 0; i < 7; i++) {
    HAL_GPIO_WritePin(SEG_PORT[i], SEG_PIN[i],
                      ((code >> i) & 1) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }
}

static void disableAll7SEG(void) {
  for (int i = 0; i < 4; i++) {
    HAL_GPIO_WritePin(EN_PORT[i], EN_PIN[i], GPIO_PIN_SET);  // PNP tắt khi EN ở mức CAO
  }
}

void update7SEG(int index) {
  disableAll7SEG();
  switch (index) {
    case 0:
      display7SEG(led_buffer[0]);
      HAL_GPIO_WritePin(EN0_GPIO_Port, EN0_Pin, GPIO_PIN_RESET);
      break;
    case 1:
      display7SEG(led_buffer[1]);
      HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, GPIO_PIN_RESET);
      break;
    case 2:
      display7SEG(led_buffer[2]);
      HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, GPIO_PIN_RESET);
      break;
    case 3:
      display7SEG(led_buffer[3]);
      HAL_GPIO_WritePin(EN3_GPIO_Port, EN3_Pin, GPIO_PIN_RESET);
      break;
    default:
      break;
  }
}

void updateClockBuffer(void) {
  led_buffer[0] = hour / 10;
  led_buffer[1] = hour % 10;
  led_buffer[2] = minute / 10;
  led_buffer[3] = minute % 10;
}

/* ================= LED matrix (Ex9) ================= */
static void matrixAllOff(void) {
  GPIO_PinState colOff = (matrix_col_on == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
  for (int i = 0; i < 8; i++) {
    HAL_GPIO_WritePin(ENM_PORT[i], ENM_PIN[i], colOff);
  }
}

/* Dữ liệu cho đường quét thứ k (có hỗ trợ xoay 90 độ nếu matrix nối ngược hàng/cột) */
static uint8_t getLineData(int k) {
#if MATRIX_TRANSPOSE
  uint8_t d = 0;
  for (int j = 0; j < 8; j++) {
    if ((matrix_buffer[j] >> k) & 1) d |= (uint8_t)(1 << j);
  }
  return d;
#else
  return matrix_buffer[k];
#endif
}

/* Bật duy nhất cột index, xuất dữ liệu cột đó ra 8 đường ROW */
void updateLEDMatrix(int index) {
  if (index < 0 || index >= MAX_LED_MATRIX) return;
  GPIO_PinState rowOff = (matrix_row_on == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;

  int line = MATRIX_LINE_REVERSE ? (7 - index) : index;  // đường ENM thật sẽ bật

  matrixAllOff();                       // 1. tắt hết đường quét để không bị bóng mờ
  uint8_t data = getLineData(index);
  for (int r = 0; r < 8; r++) {         // 2. xuất dữ liệu ra 8 đường ROW
    int pin = MATRIX_ROW_REVERSE ? (7 - r) : r;
    HAL_GPIO_WritePin(ROW_PORT[pin], ROW_PIN[pin], ((data >> r) & 1) ? matrix_row_on : rowOff);
  }
  HAL_GPIO_WritePin(ENM_PORT[line], ENM_PIN[line], matrix_col_on);    // 3. bật đường quét
}

/* ================= Animation (Ex10) ================= */
/* Cửa sổ 8 cột trượt trên khung 16 cột -> chữ "A" chạy sang trái rồi quay vòng */
void shiftLeftAnimation(void) {
  for (int i = 0; i < 8; i++) {
    matrix_buffer[i] = anim_frame[(i + anim_offset) % 16];
  }
  anim_offset = (anim_offset + 1) % 16;
}

/* ================= Dò phân cực matrix ================= */
/* Tổ hợp: 1 = ENM thấp/ROW thấp, 2 = ENM cao/ROW cao, 3 = ENM thấp/ROW cao, 4 = ENM cao/ROW thấp */
void applySelftestCombo(int combo) {
  static const GPIO_PinState COL[4] = {GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET};
  static const GPIO_PinState ROW[4] = {GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET,   GPIO_PIN_RESET};
  matrix_col_on = COL[combo];
  matrix_row_on = ROW[combo];
  for (int i = 0; i < 4; i++) led_buffer[i] = combo + 1;
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
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  disableAll7SEG();
#if MATRIX_SELFTEST
  applySelftestCombo(0);
  setTimer(T_SELFTEST, 5000);
#else
  updateClockBuffer();
#if EXERCISE == 10
  shiftLeftAnimation();
#endif
#endif
  matrixAllOff();

  setTimer(T_CLOCK, 1000);
  setTimer(T_SCAN_7SEG, SCAN_7SEG_MS);
  setTimer(T_SCAN_MATRIX, SCAN_MATRIX_MS);
  setTimer(T_SHIFT, SHIFT_MS);
  setTimer(T_LED_RED, 1000);             // Program 1.8 (Ex6)

  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Ex7: đồng hồ + DOT chạy bằng software timer, không còn HAL_Delay */
    if (timer_flag[T_CLOCK] == 1) {
      setTimer(T_CLOCK, 1000);
      second++;
      if (second >= 60) { second = 0; minute++; }
      if (minute >= 60) { minute = 0; hour++; }
      if (hour >= 24)   { hour = 0; }
#if !MATRIX_SELFTEST
      updateClockBuffer();
#endif
      HAL_GPIO_TogglePin(DOT_GPIO_Port, DOT_Pin);
    }

    /* Ex8: quét 7-seg chuyển từ ngắt về main */
    if (timer_flag[T_SCAN_7SEG] == 1) {
      setTimer(T_SCAN_7SEG, SCAN_7SEG_MS);
      update7SEG(index_led++);
      if (index_led >= MAX_LED) index_led = 0;
    }

    /* Ex9: quét LED matrix trong main */
    if (timer_flag[T_SCAN_MATRIX] == 1) {
      setTimer(T_SCAN_MATRIX, SCAN_MATRIX_MS);
      updateLEDMatrix(index_led_matrix++);
      if (index_led_matrix >= MAX_LED_MATRIX) index_led_matrix = 0;
    }

#if MATRIX_SELFTEST
    if (timer_flag[T_SELFTEST] == 1) {
      setTimer(T_SELFTEST, 5000);
      selftest_combo = (selftest_combo + 1) % 4;
      matrixAllOff();                       // tắt theo phân cực cũ trước khi đổi
      applySelftestCombo(selftest_combo);
    }
#elif EXERCISE == 10
    /* Ex10: dịch chữ sang trái */
    if (timer_flag[T_SHIFT] == 1) {
      setTimer(T_SHIFT, SHIFT_MS);
      shiftLeftAnimation();
    }
#endif

    /* Ex6: ví dụ Program 1.8 - LED đỏ PA5 */
    if (timer_flag[T_LED_RED] == 1) {
      HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
      setTimer(T_LED_RED, 2000);
    }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, ENM0_Pin|ENM1_Pin|DOT_Pin|LED_RED_Pin
                          |EN0_Pin|EN1_Pin|EN2_Pin|EN3_Pin
                          |ENM2_Pin|ENM3_Pin|ENM4_Pin|ENM5_Pin
                          |ENM6_Pin|ENM7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SEG0_Pin|SEG1_Pin|SEG2_Pin|ROW2_Pin
                          |ROW3_Pin|ROW4_Pin|ROW5_Pin|ROW6_Pin
                          |ROW7_Pin|SEG3_Pin|SEG4_Pin|SEG5_Pin
                          |SEG6_Pin|ROW0_Pin|ROW1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : ENM0_Pin ENM1_Pin DOT_Pin LED_RED_Pin
                           EN0_Pin EN1_Pin EN2_Pin EN3_Pin
                           ENM2_Pin ENM3_Pin ENM4_Pin ENM5_Pin
                           ENM6_Pin ENM7_Pin */
  GPIO_InitStruct.Pin = ENM0_Pin|ENM1_Pin|DOT_Pin|LED_RED_Pin
                          |EN0_Pin|EN1_Pin|EN2_Pin|EN3_Pin
                          |ENM2_Pin|ENM3_Pin|ENM4_Pin|ENM5_Pin
                          |ENM6_Pin|ENM7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SEG0_Pin SEG1_Pin SEG2_Pin ROW2_Pin
                           ROW3_Pin ROW4_Pin ROW5_Pin ROW6_Pin
                           ROW7_Pin SEG3_Pin SEG4_Pin SEG5_Pin
                           SEG6_Pin ROW0_Pin ROW1_Pin */
  GPIO_InitStruct.Pin = SEG0_Pin|SEG1_Pin|SEG2_Pin|ROW2_Pin
                          |ROW3_Pin|ROW4_Pin|ROW5_Pin|ROW6_Pin
                          |ROW7_Pin|SEG3_Pin|SEG4_Pin|SEG5_Pin
                          |SEG6_Pin|ROW0_Pin|ROW1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
/* Ex8: ngắt chỉ còn chạy software timer */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM2) {
    timer_run();
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
