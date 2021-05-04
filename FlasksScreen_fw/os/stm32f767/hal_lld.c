#include "hal.h"

void hal_lld_init(void) {

  /* Reset of all peripherals. AHB3 is not reseted because it could have
     been initialized in the board initialization file (board.c).
     Note, GPIOs are not reset because initialized before this point in
     board files.*/
  rccResetAHB1(~STM32_GPIO_EN_MASK);
  rccResetAHB2(~0);
  rccResetAPB1(~RCC_APB1RSTR_PWRRST);
  rccResetAPB2(~0);

  /* DMA subsystems initialization.*/
#if defined(STM32_DMA_REQUIRED)
  dmaInit();
#endif

  /* IRQ subsystem initialization.*/
  irqInit();

#if STM32_SRAM2_NOCACHE
  /* The SRAM2 bank can optionally made a non cache-able area for use by
     DMA engines.*/
  mpuConfigureRegion(MPU_REGION_7,
                     SRAM2_BASE,
                     MPU_RASR_ATTR_AP_RW_RW |
                     MPU_RASR_ATTR_NON_CACHEABLE |
                     MPU_RASR_SIZE_16K |
                     MPU_RASR_ENABLE);
  mpuEnable(MPU_CTRL_PRIVDEFENA);

  /* Invalidating data cache to make sure that the MPU settings are taken
     immediately.*/
  SCB_CleanInvalidateDCache();
#endif

  /* Programmable voltage detector enable.*/
#if STM32_PVD_ENABLE
  PWR->CR1 |= PWR_CR1_PVDE | (STM32_PLS & STM32_PLS_MASK);
#endif /* STM32_PVD_ENABLE */
}

