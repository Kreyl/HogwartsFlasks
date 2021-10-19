/*
 * board.h
 *
 *  Created on: 12 ����. 2015 �.
 *      Author: Kreyl
 */

#pragma once

// ==== General ====
#define BOARD_NAME          "HogwartsFlasks01"
#define APP_NAME            "HogwartsFlasksMain"

// MCU type as defined in the ST header.
#define STM32F767xx

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ 12000000

// OS timer settings
#define STM32_ST_IRQ_PRIORITY   2
#define STM32_ST_USE_TIMER      5
#define SYS_TIM_CLK             (Clk.GetTimInputFreq(TIM5))

//  Periphery
#define I2C1_ENABLED            TRUE
#define I2C2_ENABLED            FALSE
#define I2C3_ENABLED            FALSE
#define SIMPLESENSORS_ENABLED   FALSE
#define BUTTONS_ENABLED         FALSE

#define ADC_REQUIRED            FALSE

#if 1 // ===================== Backup regs addresses ===========================
#define BCKP_REG_SETUP_INDX     0
#define BCKP_REG_CLRH_INDX      1
#define BCKP_REG_CLRM_INDX      2
#define BCKP_REG_VOLUME_INDX    9
#define BCKP_REG_GRIF_INDX      11
#define BCKP_REG_SLYZ_INDX      12
#define BCKP_REG_RAVE_INDX      13
#define BCKP_REG_HUFF_INDX      14

#endif

#if 1 // ========================== GPIO =======================================
// EXTI
#define INDIVIDUAL_EXTI_IRQ_REQUIRED    FALSE

// LED
#define LED_PIN         GPIOF, 7, omPushPull

// Neopixel LEDs
#define NPX_LED_CNT     516
#define NPX_SPI         SPI5
#define NPX_GPIO        GPIOF
#define NPX_PIN         9
#define NPX_AF          AF5

#define NPX2_LED_CNT    25  // See Mirilli.h
#define NPX2_SPI        SPI1
#define NPX2_GPIO       GPIOB
#define NPX2_PIN        5
#define NPX2_AF         AF5

// ==== Lora ====
#define SX_SPI          SPI2
#define SX_SCK          GPIOB, 13, omPushPull, pudNone, AF5
#define SX_MISO         GPIOC,  2, omPushPull, pudNone, AF5
#define SX_MOSI         GPIOC,  3, omPushPull, pudNone, AF5
#define SX_NSS          GPIOB, 12
#define SX_NRESET       GPIOC, 10
#define SX_DIO0_GPIO    GPIOA
#define SX_DIO0_PIN     0
#define SX_DIO1_GPIO    GPIOC
#define SX_DIO1_PIN     1
#define SX_DIO2_GPIO    GPIOE
#define SX_DIO2_PIN     2
#define SX_DIO3_GPIO    GPIOE
#define SX_DIO3_PIN     3

// UART
#define UART_TX_PIN     GPIOG, 14
#define UART_RX_PIN     GPIOG, 9

// RS485
#define RS485_TXEN      GPIOC, 8, AF7
#define RS485_TX        GPIOC, 12
#define RS485_RX        GPIOD, 2

// SD
#define SDC_DRV         SDCD2
#define SD_PWR_PIN      GPIOB, 2
#define SD_DAT0         GPIOB, 14, omPushPull, pudPullUp, AF10
#define SD_DAT1         GPIOB, 15, omPushPull, pudPullUp, AF10
#define SD_DAT2         GPIOB,  3, omPushPull, pudPullUp, AF10
#define SD_DAT3         GPIOB,  4, omPushPull, pudPullUp, AF10
#define SD_CLK          GPIOD,  6, omPushPull, pudNone,   AF11
#define SD_CMD          GPIOD,  7, omPushPull, pudPullUp, AF11

// USB
#define USB_DETECT_PIN  GPIOA, 9
#define USB_DM          GPIOA, 11
#define USB_DP          GPIOA, 12
#define USB_AF          AF10
//#define BOARD_OTG_NOVBUSSENS    TRUE

// CS42L52
#define AU_RESET        GPIOC, 11, omPushPull
#define AU_MCLK         GPIOA, 8, omPushPull, pudNone, AF0  // MCO
#define AU_SCLK         GPIOD, 13, omPushPull, pudNone, AF10 // SCK;  SAI2_SCK_A
#define AU_SDIN         GPIOD, 11, omPushPull, pudNone, AF10 // MOSI; SAI2_SD_A
#define AU_LRCK         GPIOD, 12, omPushPull, pudNone, AF10 // LRCK; SAI2_FS_A
#define AU_i2c          i2c1
#define AU_SAI          SAI2
#define AU_SAI_A        SAI2_Block_A
#define AU_SAI_B        SAI2_Block_B
#define AU_SAI_RccEn()  RCC->APB2ENR |= RCC_APB2ENR_SAI2EN
#define AU_SAI_RccDis() RCC->APB2ENR &= ~RCC_APB2ENR_SAI2EN

// I2C
#define I2C1_GPIO       GPIOB
#define I2C1_SCL        6
#define I2C1_SDA        7
#define I2C_AF          AF4
#define I2C_BAUDRATE_HZ 400000
// i2cclkPCLK1, i2cclkSYSCLK, i2cclkHSI
#define I2C_CLK_SRC     i2cclkPCLK1

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
#define I2C_USE_DMA         TRUE

// === DMA1 ===
#define RS485_DMA_RX    STM32_DMA_STREAM_ID(1, 0)
#define SPI2_DMA_RX     STM32_DMA_STREAM_ID(1, 1)
#define MEMCPY_DMA      STM32_DMA_STREAM_ID(1, 2)
#define SPI2_DMA_TX     STM32_DMA_STREAM_ID(1, 4)
#define I2C1_DMA_RX     STM32_DMA_STREAM_ID(1, 5)
#define I2C1_DMA_TX     STM32_DMA_STREAM_ID(1, 6)
#define RS485_DMA_TX    STM32_DMA_STREAM_ID(1, 7)

// Channels DMA1
#define SPI2TX_DMA_CHNL 0
#define I2C1_DMA_CHNL   1
#define RS485_DMA_CHNL  4
#define SPI2RX_DMA_CHNL 9

// === DMA2 ===
#define STM32_SDC_SDMMC2_DMA_STREAM STM32_DMA_STREAM_ID(2, 0)
#define UART_DMA_RX     STM32_DMA_STREAM_ID(2, 1)
#define SAI_DMA_A       STM32_DMA_STREAM_ID(2, 2)
#define JPEG_DMA_IN     STM32_DMA_STREAM_ID(2, 3)
#define JPEG_DMA_OUT    STM32_DMA_STREAM_ID(2, 4)
#define NPX2_DMA        STM32_DMA_STREAM_ID(2, 5)  // SPI1 TX
#define NPX_DMA         STM32_DMA_STREAM_ID(2, 6)  // SPI5 TX
#define UART_DMA_TX     STM32_DMA_STREAM_ID(2, 7)

// Channels DMA2
#define NPX2_DMA_CHNL   3
#define UART_DMA_CHNL   5
#define NPX_DMA_CHNL    7
#define JPEG_DMA_CHNL   9
#define SAI_DMA_CHNL    10
#define SDMMC_DMA_CHNL  11

// Modes
#define MEMCPY_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_HIGH | STM32_DMA_CR_MSIZE_WORD | STM32_DMA_CR_PSIZE_WORD | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2M)
#define UART_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)


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
#define UART_TXBUF_SZ   2048
#define UART_RXBUF_SZ   99

#define CMD_UART        USART6

#define CMD_UART_PARAMS \
    CMD_UART, UART_TX_PIN, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL), \
    uartclkHSI

#define RS485_PARAMS \
    UART5, RS485_TX, RS485_RX, \
    RS485_DMA_TX, RS485_DMA_RX, UART_DMA_TX_MODE(RS485_DMA_CHNL), UART_DMA_RX_MODE(RS485_DMA_CHNL), \
    uartclkHSI // Use independent clock

#endif
