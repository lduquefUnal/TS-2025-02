/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  * Luis Santiago Duque F
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
#include "fsm.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
DMA_HandleTypeDef hdma_tim3_ch2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
const uint32_t timer_clk = 84000000UL;
static uint16_t fft_size = 1024;
static uint16_t display_value   =  0;
static uint8_t nextDigit_FSM    =  1;
static uint8_t tx_buffer[256]   = {0};
#define FREQ_BUFFER_SIZE 16
float freq_buffer[FREQ_BUFFER_SIZE] = {0};
#define NUM_MUESTRAS 1024
uint16_t adc_buffer[2 * NUM_MUESTRAS];
uint16_t vrx_buffer[NUM_MUESTRAS];
uint16_t vry_buffer[NUM_MUESTRAS];

char rx_buffer[128] = {0};
uint8_t rx_char = 0;
uint8_t rx_index=0;
volatile e_PosiblesEvents pending_event = IDLE;
volatile uint8_t data_snapshot = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
void displayNumber(uint8_t digitValue);
void HandleLEDDelayCmd(const char *arg);
void HandleSampleFreqCmd(const char *arg);
void HandleRGBCmd(const char *arg);
void HandlePWMFreqCmd(const char *arg);
void HandleUnknownCmd(void);
void HandleStatusCmd(void);
void HandlePrintADC(void);
void HandleClearCmd(void) ;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

e_PosiblesStates state_machine_action(e_PosiblesEvents event) {
    switch (event) {
    	case IDLE:
    		break;
        case EVENT_ENCODER:
            if (data_snapshot)  // DATA snapshot
                display_value = (display_value == 0) ? 4095 : display_value - 1;
            else
                display_value = (display_value == 4095) ? 0 : display_value + 1;
            break;

        case EVENT_SW:
            display_value = 0;
            nextDigit_FSM = 1;
            break;

        case EVENT_TIMER_TICK:{
        	uint8_t x_val = (vrx_buffer[0] * 99) / 4095;
        	uint8_t y_val = (vry_buffer[0] * 99) / 4095;
        	display_value = (y_val * 100) + x_val;
            // Apagar todos los dígitos
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);  // userDis1
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);  // userDis2
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);  // userDis3
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // userDis4

            switch (nextDigit_FSM) {
                case 1:
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
                    displayNumber(display_value % 10);
                    break;
                case 2:
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
                    displayNumber((display_value / 10) % 10);
                    break;
                case 3:
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
                    displayNumber((display_value / 100) % 10);
                    break;
                case 4:
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
                    displayNumber((display_value / 1000) % 10);
                    break;
            }
            nextDigit_FSM = (nextDigit_FSM < 4) ? nextDigit_FSM + 1 : 1;
            break;}
        case EVENT_USART: {
            // eco
            int len = snprintf((char*)tx_buffer, sizeof(tx_buffer),
                               "Comando recibido: %s\r\n", rx_buffer);
            HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);

            // dispatch
            if      (strncmp((char*)rx_buffer,"led=",4)==0)     HandleLEDDelayCmd(rx_buffer+4);
            else if (strncmp((char*)rx_buffer,"fmuestreo=",10)==0)    HandleSampleFreqCmd(rx_buffer+10);
            else if (strncmp((char*)rx_buffer,"rgb=",4)==0)     HandleRGBCmd(rx_buffer+4);
            else if (strncmp((char*)rx_buffer,"status",6)==0) HandleStatusCmd();
            else if (strncmp((char*)rx_buffer,"help",4)==0) HandleHelpCmd();
            else if (strncmp((char*)rx_buffer, "clear", 5) == 0) HandleClearCmd();
            else                                                ;

            memset(rx_buffer,0,sizeof(rx_buffer));
            break;
        }return event;
    }
}

void displayNumber(uint8_t digitValue) {
    uint32_t start = DWT->CYCCNT;
    static uint8_t contador_muestras = 0;
    // Apagar todos los segmentos
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);  // A
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);  // B
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);  // C
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);   // D
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);   // E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);   // F
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);   // G

    switch (digitValue) {
        case 0:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            break;
        case 1:
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            break;
        case 2:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
        case 3:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
        case 4:
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
        case 5:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
        case 6:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
        case 7:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            break;
        case 8:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
        case 9:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            break;
    }
    uint32_t elapsed = DWT->CYCCNT - start;

    if (contador_muestras < 10) {
        printf("Ciclos para mostrar digito %d: %lu\n", digitValue, (unsigned long)elapsed);
        contador_muestras++;
    }
}
void HandleLEDDelayCmd(const char *arg) {
    uint32_t nuevo = atoi(arg);
    // Ajusta TIM2 para el blinky
    __HAL_TIM_SET_AUTORELOAD(&htim2, nuevo);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    // Feedback
    int len = snprintf((char*)tx_buffer, sizeof(tx_buffer),
                       "LED delay = %lu ms\r\n", (unsigned long)nuevo);
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
}

void HandleSampleFreqCmd(const char *arg) {
	uint32_t fs;
	char option = arg[0];
    // Verifica la opción seleccionada (1, 2, 3, 4)
	switch(option) {
	        case '1': fs = 44100; break;
	        case '2': fs = 48000; break;
	        case '3': fs = 96000; break;
	        case '4': fs = 128000; break;
	        default: {
	            const char *msg = "Opciones válidas para 'fmuestreo=' son: 1, 2, 3, 4\r\n";
	            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
	            return;
	        }
	    }
    // calcular ARR y PSC
    uint32_t arr = timer_clk / fs - 1;
    uint32_t psc = 0;
    if (arr > 0xFFFF) {
        // escalar PSC si supera 16 bits
        psc = (arr / 0x10000) + 1;
        arr = (timer_clk / (psc+1) / fs) - 1;
    }
    // aplicar a TIM3 y reiniciar
    __HAL_TIM_SET_PRESCALER(&htim3, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_TIM_Base_Stop(&htim3);
    HAL_TIM_Base_Start(&htim3);
    // Feedback
    int len = snprintf((char*)tx_buffer, sizeof(tx_buffer),
                       "Sample TIM3 @ %lu Hz (PSC=%lu, ARR=%lu)\r\n",
                       (unsigned long)fs, (unsigned long)psc, (unsigned long)arr);
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
}

void HandleRGBCmd(const char *arg) {
    uint32_t r, g, b;
    if (sscanf(arg, "%lu %lu %lu", &r, &g, &b) == 3) {
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, g); // G
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, b); // B
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, r); // R

        int len = snprintf((char*)tx_buffer, sizeof(tx_buffer),
                           "RGB PWM -> R=%lu, G=%lu, B=%lu\r\n", r, g, b);
        HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
    } else {
        const char *msg = "Formato inválido. Usa: rgb=R G B@\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
    }
}

void HandleStatusCmd(void) {
    uint32_t psc = htim3.Init.Prescaler;
    uint32_t arr = htim3.Init.Period;
    float fs = 84000000.0f / ((psc + 1) * (arr + 1));

    int len = snprintf((char*)tx_buffer, sizeof(tx_buffer),
    		"Config:\r\nSample TIM3 @ %.2f Hz (PSC=%lu, ARR=%lu)\r\nCanal ADC: 6 y 7\r\n",
    		fs, (unsigned long)psc, (unsigned long)arr);
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
}






void HandleClearCmd(void) {
    const char *clear_screen = "\033[2J\033[H";  // Secuencia ANSI para limpiar la pantalla
    HAL_UART_Transmit(&huart2, (uint8_t*)clear_screen, strlen(clear_screen), 1000);
}
void HandleHelpCmd(void) {
    const char *help_msg =
            "\r\n========== AYUDA =========="
            "\r\nComandos disponibles:\r\n"
            "  led=<ms>@       - Cambia la frecuencia del LED Blinky\r\n"
            "  fmuestreo=<1|2|3|4>@ - Frecuencia de muestreo del ADC:\r\n"
            "                 - 1 -> 44100 Hz\r\n"
            "                 - 2 -> 48000 Hz\r\n"
            "                 - 3 -> 96000 Hz\r\n"
            "                 - 4 -> 128000 Hz\r\n"
            "  rgb=<RGB>@      - Control de LED RGB, ej: rgb=RG\r\n"
    		"  status@         - Mostrar configuración actual\r\n"
            "  print@	          - Imprimir datos crudos del ADC\r\n"
            "  help @          - Mostrar esta ayuda\r\n"
            "===========================\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)help_msg, strlen(help_msg), 1000);
}
void imprimirBinario(uint8_t valor) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (valor >> i) & 1);
    }
}

void mostrarOperacion(const char *desc, uint8_t resultado) {
    printf("%s\n", desc);
    printf("Binario: "); imprimirBinario(resultado);
    printf("\nHex: 0x%02X\n", resultado);
    printf("ASCII: %c\n\n", (resultado >= 32 && resultado <= 126) ? resultado : '.');
}

void verificarOperacionesCrucigrama(void) {
    // 1. Caracter ASCII 'B'
    uint8_t op1 = 'B';
    mostrarOperacion("1. ASCII 'B'", op1);

    // 2. ((!!0xC) << 0b11) | (~0x45)
    uint8_t op2 = ((!!0xC) << 0b11) | (~0x45);
    mostrarOperacion("2. ((!!0xC) << 0b11) | (~0x45)", op2);

    // 3. ((0b00111100 & 0xAC) << 0x3)
    uint8_t op3 = (0b00111100 & 0xAC) << 0x3;
    mostrarOperacion("3. ((0b00111100 & 0xAC) << 0x3)", op3);

    // 4. -121
    uint8_t op4 = (uint8_t)(-121);
    mostrarOperacion("4. -121 (uint8_t)", op4);

    // 5. (0xCAFE & 0x0FF0) >> 7
    uint16_t val5 = (0xCAFE & 0x0FF0) >> 7;
    uint8_t op5 = (uint8_t)(val5 & 0xFF);
    mostrarOperacion("5. (0xCAFE & 0x0FF0) >> 7", op5);

    // 6. (1 << 4) + (0x3 << 4) - 10 + (0b11 << 0x6)
    uint8_t op6 = (1 << 4) + (0x3 << 4) - 10 + (0b11 << 6);
    mostrarOperacion("6. (1<<4)+(0x3<<4)-10+(0b11<<6)", op6);

    // 7. ('T' | '5')
    uint8_t op7 = 'T' | '5';
    mostrarOperacion("7. ('T' | '5')", op7);

    // 8. (2921 & 0xD6)
    uint8_t op8 = 2921 & 0xD6;
    mostrarOperacion("8. (2921 & 0xD6)", op8);

    // 9. 01100110 ^ 00101011
    uint8_t a9 = 0b01100110;
    uint8_t b9 = 0b00101011;
    uint8_t op9 = a9 ^ b9;
    mostrarOperacion("9. 01100110 ^ 00101011", op9);
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
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Habilitar DWT
	DWT->CYCCNT = 0;                                // Reiniciar contador
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Encender contador de ciclos
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_TIM_Base_Start_IT(&htim4);
  HAL_UART_Receive_IT(&huart2,&rx_char,1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, fft_size);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);
  HandleSampleFreqCmd(3); //96000 hz muestreo

  verificarOperacionesCrucigrama();
  medirTiempoEnvioUART();
  HandleHelpCmd();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  	if (pending_event != IDLE){
  		state_machine_action(pending_event);
  		pending_event = IDLE;
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
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 2;
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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 83;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 255;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); // PA8 - g
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2); // PA9 - B
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3); // PA10 - R
  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  htim2.Init.Prescaler = 42000-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 500;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 8;
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);
  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 840-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 200-1;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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
  HAL_GPIO_WritePin(GPIOC, LedB_Pin|segD_Pin|segE_Pin|LedR_Pin
                          |LedG_Pin|segA_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(userLed_GPIO_Port, userLed_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, dis1_Pin|dis2_Pin|dis4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, dis3_Pin|segF_Pin|segB_Pin|segC_Pin
                          |segG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LedB_Pin segD_Pin segE_Pin LedR_Pin
                           LedG_Pin segA_Pin */
  GPIO_InitStruct.Pin = LedB_Pin|segD_Pin|segE_Pin|LedR_Pin
                          |LedG_Pin|segA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : userLed_Pin */
  GPIO_InitStruct.Pin = userLed_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(userLed_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : dis1_Pin dis2_Pin dis4_Pin */
  GPIO_InitStruct.Pin = dis1_Pin|dis2_Pin|dis4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : dis3_Pin segF_Pin segB_Pin segC_Pin
                           segG_Pin */
  GPIO_InitStruct.Pin = dis3_Pin|segF_Pin|segB_Pin|segC_Pin
                          |segG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : userData_Pin */
  GPIO_InitStruct.Pin = userData_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(userData_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : userSw_Pin */
  GPIO_InitStruct.Pin = userSw_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(userSw_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : userClk_Pin */
  GPIO_InitStruct.Pin = userClk_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(userClk_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void medirTiempoEnvioUART(void) {
    const char *msg = "Hola mundo! - Examen Taller V\n";
    uint32_t start = DWT->CYCCNT;

    printf("%s", msg);

    uint32_t elapsed = DWT->CYCCNT - start;
    printf("Ciclos para enviar mensaje UART: %lu\n", (unsigned long)elapsed);
}
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim->Instance==TIM2){
		HAL_GPIO_TogglePin(userLed_GPIO_Port,userLed_Pin);
	}else if(htim->Instance==TIM4){
			if (pending_event == IDLE)
			pending_event = EVENT_TIMER_TICK;
	}else if (htim->Instance==TIM3){


	}
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == userClk_Pin) {
        data_snapshot = HAL_GPIO_ReadPin(userData_GPIO_Port, userData_Pin);
        if (pending_event == IDLE)
            pending_event = EVENT_ENCODER;
    } else if (GPIO_Pin == userSw_Pin) {
        if (pending_event == IDLE)
            pending_event = EVENT_SW;
    }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart->Instance == USART2){
		if(rx_index<sizeof(rx_buffer)-1){
			rx_buffer[rx_index++]=rx_char;
		}
        if (rx_char == '@') {
			rx_buffer[rx_index]='\0';
			rx_index=0;
			pending_event = EVENT_USART; //aquí levanta tu bandera
		}
		HAL_UART_Receive_IT(huart, &rx_char,1);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        for (int i = 0; i < NUM_MUESTRAS; i++) {
            vrx_buffer[i] = adc_buffer[2 * i];       // Canal 6
            vry_buffer[i] = adc_buffer[2 * i + 1];   // Canal 7
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
