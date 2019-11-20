/*
 * board.h
 *
 *  Created on: 12 ����. 2015 �.
 *      Author: Kreyl
 */

#pragma once

// ==== General ====
#define BOARD_NAME          "HogwartsFlasks01"
#define APP_NAME            "FlasksScreen"

// MCU type as defined in the ST header.
#define STM32F767xx

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ 12000000

// OS timer settings
#define STM32_ST_IRQ_PRIORITY   2
#define STM32_ST_USE_TIMER      5
#define SYS_TIM_CLK             (Clk.GetTimInputFreq(TIM5))

//  Periphery
#define I2C1_ENABLED            FALSE
#define I2C2_ENABLED            FALSE
#define I2C3_ENABLED            FALSE
#define SIMPLESENSORS_ENABLED   FALSE
#define BUTTONS_ENABLED         FALSE

#define ADC_REQUIRED            FALSE

#if 1 // ========================== GPIO =======================================
// EXTI
#define INDIVIDUAL_EXTI_IRQ_REQUIRED    FALSE

// LED
#define LED_PIN         GPIOF, 7, omPushPull

// LCD
#define LCD_BCKLT       GPIOF, 6, TIM10, 1, invNotInverted, omPushPull, 99
#define LCD_DISP        GPIOD, 4

// UART
#define UART_TX_PIN     GPIOG, 14
#define UART_RX_PIN     GPIOG, 9


#endif // GPIO

#if 1 // =========================== SPI =======================================
#endif
#if I2C2_ENABLED // ====================== I2C ================================
#define I2C2_BAUDRATE   400000
#define I2C_PILL        i2c2
#endif

#if ADC_REQUIRED // ======================= Inner ADC ==========================
// Measuremen timing src
#define ADC_TIM             TIM2

// ADC channels
#define ADC_VOLTAGE_CH      4
#define ADC_CURRENT_CH      6
#define ADC_VREFINT_CH      17 // Constant

#define ADC_CHANNELS        { ADC_VOLTAGE_CH, ADC_CURRENT_CH, ADC_VREFINT_CH }
// Index in array
#define ADC_VOLTAGE_INDX    0
#define ADC_CURRENT_INDX    1
#define ADC_VREFINT_INDX    2

#define ADC_CHANNEL_CNT     3   // Do not use countof(AdcChannels) as preprocessor does not know what is countof => cannot check
#endif

#if 1 // =========================== DMA =======================================
#define STM32_DMA_REQUIRED  TRUE
// ==== Uart ====
// Remap is made automatically if required
#define UART_DMA_TX     STM32_DMA_STREAM_ID(2, 6)
#define UART_DMA_RX     STM32_DMA_STREAM_ID(2, 1)
#define UART_DMA_CHNL   5
#define UART_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)

// NPX
#define NPX1_DMA        STM32_DMA_STREAM_ID(2, 4)  // SPI5 TX
#define NPX1_DMA_CHNL   2
#define NPX2_DMA        STM32_DMA_STREAM_ID(2, 3)  // SPI1 TX
#define NPX2_DMA_CHNL   3

#if I2C1_ENABLED // ==== I2C1 ====
#define I2C1_DMA_TX     STM32_DMA1_STREAM6
#define I2C1_DMA_RX     STM32_DMA1_STREAM5
#endif
#if I2C2_ENABLED // ==== I2C2 ====
#define I2C2_DMA_TX     STM32_DMA1_STREAM7
#define I2C2_DMA_RX     STM32_DMA1_STREAM3
#endif

// ==== SDMMC ====
#define STM32_SDC_SDMMC2_DMA_STREAM     STM32_DMA_STREAM_ID(2, 0)


#if ADC_REQUIRED
#define ADC_DMA         STM32_DMA_STREAM_ID(2, 0)
#define ADC_DMA_MODE    STM32_DMA_CR_CHSEL(0) |   /* DMA2 Stream0 Channel0 */ \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_HWORD | \
                        STM32_DMA_CR_PSIZE_HWORD | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                        STM32_DMA_CR_CIRC |       /* Circulate */ \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */
#endif // ADC

#endif // DMA

#if 1 // ========================== USART ======================================
#define PRINTF_FLOAT_EN TRUE
#define UART_TXBUF_SZ   8192
#define UART_RXBUF_SZ   99

#define UARTS_CNT       1

#define CMD_UART_PARAMS \
    USART6, UART_TX_PIN, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL), \
    uartclkHSI

#endif
