/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    STM32F7xx/hal_lld.h
 * @brief   STM32F7xx HAL subsystem low level driver header.
 * @pre     This module requires the following macros to be defined in the
 *          @p board.h file:
 *          - STM32_LSECLK.
 *          - STM32_LSEDRV.
 *          - STM32_LSE_BYPASS (optionally).
 *          - STM32_HSECLK.
 *          - STM32_HSE_BYPASS (optionally).
 *          - STM32_VDD (as hundredths of Volt).
 *          .
 *          One of the following macros must also be defined:
 *          - STM32F722xx, STM32F723xx very high-performance MCUs.
 *          - STM32F732xx, STM32F733xx very high-performance MCUs.
 *          - STM32F745xx, STM32F746xx, STM32F756xx very high-performance MCUs.
 *          - STM32F765xx, STM32F767xx, STM32F769xx very high-performance MCUs.
 *          - STM32F777xx, STM32F779xx very high-performance MCUs.
 *          .
 *
 * @addtogroup HAL
 * @{
 */

#ifndef HAL_LLD_H
#define HAL_LLD_H

#include "board.h"
#include "mcuconf.h"
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
 * @name    Internal clock sources
 * @{
 */
#define STM32_HSICLK            16000000    /**< High speed internal clock. */
#define STM32_LSICLK            32000       /**< Low speed internal clock.  */
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   Disables the PWR/RCC initialization in the HAL.
 */
#if !defined(STM32_NO_INIT) || defined(__DOXYGEN__)
#define STM32_NO_INIT                       FALSE
#endif

#if !defined(STM32F7xx_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F7xx_MCUCONF not defined"
#endif

#if defined(STM32F722xx) && !defined(STM32F722_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F722_MCUCONF not defined"
#endif

#if defined(STM32F732xx) && !defined(STM32F732_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F732_MCUCONF not defined"
#endif

#if defined(STM32F723xx) && !defined(STM32F723_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F723_MCUCONF not defined"
#endif

#if defined(STM32F733xx) && !defined(STM32F733_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F733_MCUCONF not defined"
#endif

#if defined(STM32F746xx) && !defined(STM32F746_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F746_MCUCONF not defined"
#endif

#if defined(STM32F756xx) && !defined(STM32F756_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F756_MCUCONF not defined"
#endif

#if defined(STM32F765xx) && !defined(STM32F765_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F765_MCUCONF not defined"
#endif

#if defined(STM32F767xx) && !defined(STM32F767_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F767_MCUCONF not defined"
#endif

#if defined(STM32F777xx) && !defined(STM32F777_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F777_MCUCONF not defined"
#endif

#if defined(STM32F769xx) && !defined(STM32F769_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F769_MCUCONF not defined"
#endif

#if defined(STM32F779xx) && !defined(STM32F779_MCUCONF)
#error "Using a wrong mcuconf.h file, STM32F779_MCUCONF not defined"
#endif

/* Various helpers.*/
#include "nvic.h"
#include "cache.h"
#include "mpu_v7m.h"
#include "stm32_isr.h"
#include "stm32_dma.h"
#include "stm32_exti.h"
#include "stm32_rcc.h"

#ifdef __cplusplus
extern "C" {
#endif
  void hal_lld_init(void);
#ifdef __cplusplus
}
#endif

#endif /* HAL_LLD_H */

/** @} */
