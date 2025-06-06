/*
 * gpio_driver_hal.h
 *
 *  Created on: May 22, 2025
 *      Author: luisduquefranco
 */

#ifndef GPIO_DRIVER_HAL_H
#define GPIO_DRIVER_HAL_H

#include <stdint.h>
#include <stm32f4xx.h>

/* Descripción de cada uno de los registros del periférico (no es necesario para esta tarea) */

/**
 * GPIO bit SET and bit RESET enumeration
 */
enum
{
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
};

/* Valores estándar para las configuraciones */
/* 8.4.1 GPIOx_MODER (dos bit por cada PIN) */
enum
{
    GPIO_MODE_IN = 0,
    GPIO_MODE_OUT,
    GPIO_MODE_ALTFN,
    GPIO_MODE_ANALOG
};

/* 8.4.2 GPIOx_OTYPER (un bit por PIN) */
enum
{
	GPIO_OTYPE_PUSHPULL = 0,
	GPIO_OTYPE_OPENDRAIN
};

/* 8.4.3 GPIOx_OSPEEDR (dos bit por cada PIN) */
enum
{
GPIO_OSPEED_LOW = 0,
GPIO_OSPEED_MEDIUM,
GPIO_OSPEED_FAST,
GPIO_OSPEED_HIGH
};

/* 8.4.4 GPIOx_PUPDR (dos bit por cada PIN) */
enum
{
GPIO_PUPDR_NOTHING = 0,
GPIO_PUPDR_PULLUP,
GPIO_PUPDR_PULLDOWN,
GPIO_PUPDR_RESERVED
};

/* 8.4.5 GPIOx_IDR (un bit por PIN) - este es el registro para leer el estado de un PIN */
/* 8.4.6 GPIOx_ODR (un bit por PIN) - este es el registro para escribir el estado de un PIN (1 o 0).
Este registro puede ser escrito y leído desde el software, pero no garantiza una escritura "atómica",
por lo cual es preferible utilizar el registro BSRR */

/* Definición de los nombres de los pines */
enum
{
PIN_0 = 0,
PIN_1,
PIN_2,
PIN_3,
PIN_4,
PIN_5,
PIN_6,
PIN_7,
PIN_8,
PIN_9,
PIN_10,
PIN_11,
PIN_12,
PIN_13,
PIN_14,
PIN_15
};

/* Definición de las funciones alternativas */
enum
{
AF0  = 0b0000,
AF1  = 0b0001,
AF2  = 0b0010,
AF3  = 0b0011,
AF4  = 0b0100,
AF5  = 0b0101,
AF6  = 0b0110,
AF7  = 0b0111,
AF8  = 0b1000,
AF9  = 0b1001,
AF10 = 0b1010,
AF11 = 0b1011,
AF12 = 0b1100,
AF13 = 0b1101,
AF14 = 0b1110,
AF15 = 0b1111
};

#define GPIO_PIN_MASK     0x0FU   /* Pin mask for assert test */

/* Structure definition which will keep the configuration information for PinX */
typedef struct
{
uint8_t GPIO_PinNumber;        // Working Pin
uint8_t GPIO_PinMode;          // Config mode: input, output, analog, alternate function
uint8_t GPIO_PinOutputSpeed;   // Output speed for working pin
uint8_t GPIO_PinPuPdControl;   // Turn ON-OFF the pull-up and pull-down resistors
uint8_t GPIO_PinOutputType;    // Selects output type: push-pull or open-drain
uint8_t GPIO_PinAltFunMode;    // Type of alternate function assigned to working pin
} GPIO_PinConfig_t;

/*
* Pin Handler definition.
* This handler is used to configure the port at which the selected pin is working.
* It holds two elements:
*  - Reference to the complete port (GPIOx), to have access for the specific registers
*  - Configuration structure: GPIO_PinConfig_t
*/
typedef struct
{
    GPIO_TypeDef        *pGPIOx;     // Pointer to the port of selected PinX
    GPIO_PinConfig_t    pinConfig;   // Holds the configuration for selected PinX
} GPIO_Handler_t;

/* For testing assert parameters - checking basic configurations. */
#define IS_GPIO_PIN_ACTION(ACTION)  (((ACTION) == GPIO_PIN_RESET) || ((ACTION) == GPIO_PIN_SET))

#define IS_GPIO_PIN(PIN)            (((uint32_t)(PIN) & GPIO_PIN_MASK) != 0x00U)

#define IS_GPIO_MODE(MODE)          (((MODE) == GPIO_MODE_IN)      || \
                                     ((MODE) == GPIO_MODE_OUT)     || \
                                     ((MODE) == GPIO_MODE_ALTFN)   || \
                                     ((MODE) == GPIO_MODE_ANALOG))

#define IS_GPIO_OUTPUT_TYPE(OUTPUT) (((OUTPUT) == GPIO_OTYPE_PUSHPULL) || \
                                     ((OUTPUT) == GPIO_OTYPE_OPENDRAIN))
#define IS_GPIO_OSPEED(SPEED)       (((SPEED) == GPIO_OSPEED_LOW)    || \
                                     ((SPEED) == GPIO_OSPEED_MEDIUM) || \
                                     ((SPEED) == GPIO_OSPEED_FAST)   || \
                                     ((SPEED) == GPIO_OSPEED_HIGH))

#define IS_GPIO_PUPDR(PULL)         (((PULL) == GPIO_PUPDR_NOTHING)  || \
                                     ((PULL) == GPIO_PUPDR_PULLUP)   || \
                                     ((PULL) == GPIO_PUPDR_PULLDOWN))

/* Header definitions for the "public functions" of gpio_driver_hal */
void gpio_Config(GPIO_Handler_t *pGPIOHandler);
void gpio_WritePin(GPIO_Handler_t *pPinHandler, uint8_t newState);
void gpio_TogglePin(GPIO_Handler_t *pPinHandler);
uint32_t gpio_ReadPin(GPIO_Handler_t *pPinHandler);

#endif /* GPIO_DRIVER_HAL_H */
