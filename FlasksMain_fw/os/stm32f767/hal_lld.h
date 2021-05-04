#ifndef HAL_LLD_H
#define HAL_LLD_H

#include "stm32_registry.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Defines the support for realtime counters in the HAL.
 */
#define HAL_IMPLEMENTS_COUNTERS             TRUE

/**
 * @name    Platform identification macros
 * @{
 */
#if defined(STM32F722xx) || defined(__DOXYGEN__)
#define PLATFORM_NAME           "STM32F745 Very High Performance with DSP and FPU"

#elif defined(STM32F723xx)
#define PLATFORM_NAME           "STM32F745 Very High Performance with DSP and FPU"

#elif defined(STM32F732xx)
#define PLATFORM_NAME           "STM32F745 Very High Performance with DSP and FPU"

#elif defined(STM32F733xx)
#define PLATFORM_NAME           "STM32F745 Very High Performance with DSP and FPU"

#elif defined(STM32F745xx)
#define PLATFORM_NAME           "STM32F745 Very High Performance with DSP and FPU"

#elif defined(STM32F746xx)
#define PLATFORM_NAME           "STM32F746 Very High Performance with DSP and FPU"

#elif defined(STM32F756xx)
#define PLATFORM_NAME           "STM32F756 Very High Performance with DSP and FPU"

#elif defined(STM32F765xx)
#define PLATFORM_NAME           "STM32F767 Very High Performance with DSP and DP FPU"

#elif defined(STM32F767xx)
#define PLATFORM_NAME           "STM32F767 Very High Performance with DSP and DP FPU"

#elif defined(STM32F769xx)
#define PLATFORM_NAME           "STM32F769 Very High Performance with DSP and DP FPU"

#elif defined(STM32F777xx)
#define PLATFORM_NAME           "STM32F767 Very High Performance with DSP and DP FPU"

#elif defined(STM32F779xx)
#define PLATFORM_NAME           "STM32F769 Very High Performance with DSP and DP FPU"

#else
#error "STM32F7xx device not specified"
#endif
/** @} */

/**
 * @name    Sub-family identifier
 */
#if !defined(STM32F7XX) || defined(__DOXYGEN__)
#define STM32F7XX
#endif
/** @} */

/**
 * @name    Absolute Maximum Ratings
 * @{
 */
/**
 * @brief   Absolute maximum system clock.
 */
#define STM32_SYSCLK_MAX        216000000

/**
 * @brief   Maximum HSE clock frequency.
 */
#define STM32_HSECLK_MAX        26000000

/**
 * @brief   Maximum HSE clock frequency using an external source.
 */
#define STM32_HSECLK_BYP_MAX    50000000

/**
 * @brief   Minimum HSE clock frequency.
 */
#define STM32_HSECLK_MIN        4000000

/**
 * @brief   Minimum HSE clock frequency.
 */
#define STM32_HSECLK_BYP_MIN    1000000

/**
 * @brief   Maximum LSE clock frequency.
 */
#define STM32_LSECLK_MAX        32768

/**
 * @brief   Maximum LSE clock frequency.
 */
#define STM32_LSECLK_BYP_MAX    1000000

/**
 * @brief   Minimum LSE clock frequency.
 */
#define STM32_LSECLK_MIN        32768

/**
 * @brief   Maximum PLLs input clock frequency.
 */
#define STM32_PLLIN_MAX         2100000

/**
 * @brief   Minimum PLLs input clock frequency.
 */
#define STM32_PLLIN_MIN         950000

/**
 * @brief   Maximum PLLs VCO clock frequency.
 */
#define STM32_PLLVCO_MAX        432000000

/**
 * @brief   Minimum PLLs VCO clock frequency.
 */
#define STM32_PLLVCO_MIN        192000000

/**
 * @brief   Maximum PLL output clock frequency.
 */
#define STM32_PLLOUT_MAX        216000000

/**
 * @brief   Minimum PLL output clock frequency.
 */
#define STM32_PLLOUT_MIN        24000000

/**
 * @brief   Maximum APB1 clock frequency.
 */
#define STM32_PCLK1_MAX         (STM32_PLLOUT_MAX / 4)

/**
 * @brief   Maximum APB2 clock frequency.
 */
#define STM32_PCLK2_MAX         (STM32_PLLOUT_MAX / 2)

/**
 * @brief   Maximum SPI/I2S clock frequency.
 */
#define STM32_SPII2S_MAX        54000000
/** @} */

/**
 * @name    Internal clock sources
 * @{
 */
#define STM32_HSICLK            16000000    /**< High speed internal clock. */
#define STM32_LSICLK            32000       /**< Low speed internal clock.  */
/** @} */


/* Various helpers.*/
#include "nvic.h"
#include "cache.h"
#include "mpu_v7m.h"
#include "stm32_isr.h"
#include "stm32_dma.h"
#include "stm32_exti.h"
#include "stm32_rcc.h"
#include "stm32_tim.h"

#ifdef __cplusplus
extern "C" {
#endif
  void hal_lld_init(void);
  void stm32_clock_init(void);
#ifdef __cplusplus
}
#endif

#endif /* HAL_LLD_H */

/** @} */
