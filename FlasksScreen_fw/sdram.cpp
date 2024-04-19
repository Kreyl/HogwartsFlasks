/*
 * sdram.cpp
 *
 *  Created on: 4 2019.
 *      Author: Kreyl
 */

#include "stm32f767xx.h"
#include "sdram.h"
#include "kl_lib.h"
#include "shell.h"
#include <string>

#define FMC_SDRAM   FMC_Bank5_6

void SdramInit() {
    // Reset FMC
    RCC->AHB3RSTR |= RCC_AHB3RSTR_FMCRST;
    RCC->AHB3RSTR &= ~RCC_AHB3RSTR_FMCRST;
    // Enable clock
    RCC->AHB3ENR |= RCC_AHB3ENR_FMCEN;
#if 1 // GPIO
    // PortA
    PinSetupAlterFunc(GPIOA, 7, omPushPull, pudNone, AF12, psVeryHigh); // SDNWE
    // PortC
    PinSetupAlterFunc(GPIOC, 4, omPushPull, pudNone, AF12, psVeryHigh); // SDNE0
    PinSetupAlterFunc(GPIOC, 5, omPushPull, pudNone, AF12, psVeryHigh); // SDCKE0
    // PortD
    PinSetupAlterFunc(GPIOD, 0, omPushPull, pudNone, AF12, psVeryHigh); // D2
    PinSetupAlterFunc(GPIOD, 1, omPushPull, pudNone, AF12, psVeryHigh); // D3
    PinSetupAlterFunc(GPIOD, 8, omPushPull, pudNone, AF12, psVeryHigh); // D13
    PinSetupAlterFunc(GPIOD, 9, omPushPull, pudNone, AF12, psVeryHigh); // D14
    PinSetupAlterFunc(GPIOD, 10, omPushPull, pudNone, AF12, psVeryHigh); // D15
    PinSetupAlterFunc(GPIOD, 14, omPushPull, pudNone, AF12, psVeryHigh); // D0
    PinSetupAlterFunc(GPIOD, 15, omPushPull, pudNone, AF12, psVeryHigh); // D1
    // PortE
    PinSetupAlterFunc(GPIOE, 0, omPushPull, pudNone, AF12, psVeryHigh); // NBL0
    PinSetupAlterFunc(GPIOE, 1, omPushPull, pudNone, AF12, psVeryHigh); // NBL1
    PinSetupAlterFunc(GPIOE, 7, omPushPull, pudNone, AF12, psVeryHigh); // D4
    PinSetupAlterFunc(GPIOE, 8, omPushPull, pudNone, AF12, psVeryHigh); // D5
    PinSetupAlterFunc(GPIOE, 9, omPushPull, pudNone, AF12, psVeryHigh); // D6
    PinSetupAlterFunc(GPIOE, 10, omPushPull, pudNone, AF12, psVeryHigh); // D7
    PinSetupAlterFunc(GPIOE, 11, omPushPull, pudNone, AF12, psVeryHigh); // D8
    PinSetupAlterFunc(GPIOE, 12, omPushPull, pudNone, AF12, psVeryHigh); // D9
    PinSetupAlterFunc(GPIOE, 13, omPushPull, pudNone, AF12, psVeryHigh); // D10
    PinSetupAlterFunc(GPIOE, 14, omPushPull, pudNone, AF12, psVeryHigh); // D11
    PinSetupAlterFunc(GPIOE, 15, omPushPull, pudNone, AF12, psVeryHigh); // D12
    // PortF
    PinSetupAlterFunc(GPIOF, 0, omPushPull, pudNone, AF12, psVeryHigh); // A0
    PinSetupAlterFunc(GPIOF, 1, omPushPull, pudNone, AF12, psVeryHigh); // A1
    PinSetupAlterFunc(GPIOF, 2, omPushPull, pudNone, AF12, psVeryHigh); // A2
    PinSetupAlterFunc(GPIOF, 3, omPushPull, pudNone, AF12, psVeryHigh); // A3
    PinSetupAlterFunc(GPIOF, 4, omPushPull, pudNone, AF12, psVeryHigh); // A4
    PinSetupAlterFunc(GPIOF, 5, omPushPull, pudNone, AF12, psVeryHigh); // A5
    PinSetupAlterFunc(GPIOF, 11, omPushPull, pudNone, AF12, psVeryHigh); // SNDRAS
    PinSetupAlterFunc(GPIOF, 12, omPushPull, pudNone, AF12, psVeryHigh); // A6
    PinSetupAlterFunc(GPIOF, 13, omPushPull, pudNone, AF12, psVeryHigh); // A7
    PinSetupAlterFunc(GPIOF, 14, omPushPull, pudNone, AF12, psVeryHigh); // A8
    PinSetupAlterFunc(GPIOF, 15, omPushPull, pudNone, AF12, psVeryHigh); // A9
    // PortG
    PinSetupAlterFunc(GPIOG, 0, omPushPull, pudNone, AF12, psVeryHigh); // A10
    PinSetupAlterFunc(GPIOG, 1, omPushPull, pudNone, AF12, psVeryHigh); // A11
    PinSetupAlterFunc(GPIOG, 2, omPushPull, pudNone, AF12, psVeryHigh); // A12
    PinSetupAlterFunc(GPIOG, 4, omPushPull, pudNone, AF12, psVeryHigh); // BA0
    PinSetupAlterFunc(GPIOG, 5, omPushPull, pudNone, AF12, psVeryHigh); // BA1
    PinSetupAlterFunc(GPIOG, 8, omPushPull, pudNone, AF12, psVeryHigh); // SDCLK
    PinSetupAlterFunc(GPIOG, 15, omPushPull, pudNone, AF12, psVeryHigh); // SDNCAS
#endif
    // == FMC SDRAM Init ==
    // Config
    FMC_SDRAM->SDCR[0] =
            (0b00UL << 13) | // No HCLK clock cycle delay
            (0b1UL  << 12) | // Single read requests are managed as bursts
            (0b10UL << 10) | // SDCLK period = 2 x HCLK periods
            (0b0UL  << 9)  | // Write accesses allowed
            (0b11UL << 7)  | // CAS Length (CL) = 3 cycles (table at 2pg) (but 18ns)
            (0b1UL  << 6)  | // Four internal Banks
            (0b01UL << 4)  | // Memory data bus width: 16 bits
            (0b10UL << 2)  | // Number of row address bits: 13
            (0b01UL << 0);   // Number of column address bits: 9
    // ==== Timings ====
    if(Clk.AHBFreqHz == 80000000) { // HCLK = 80MHz => SDCLK = 40MHz => 1 cycle = 25ns
        FMC_SDRAM->SDTR[0] =
                (0b0010UL << 24) | // RCD (Row to column delay) = 3 cycles (table at 2pg) (but 18ns)
                (0b0010UL << 20) | // RP (Row precharge delay) = 3 cycles (table at 2pg) (but 18ns)
                (0b0000UL << 16) | // WR = 1 cycle (WR >= RAS-RCD and WR >= RC-RCD-RP)
                (0b0010UL << 12) | // RC = 60ns = 3 cycles
                (0b0001UL << 8)  | // RAS = 42ns = 2 cycles
                (0b0010UL << 4)  | // XSR = 67ns = 3 cycles
                (0b0000UL << 0);   // MRD = 1 CK
    }
    else if(Clk.AHBFreqHz == 216000000) { // HCLK = 216MHz => SDCLK = 108MHz => 1 cycle = 9.26ns
        FMC_SDRAM->SDTR[0] =
                (0b0010UL << 24) | // RCD (Row to column delay) = 3 cycles (table at 2pg) (but 18ns)
                (0b0010UL << 20) | // RP (Row precharge delay) = 3 cycles (table at 2pg) (but 18ns)
                (0b0000UL << 16) | // WR = 1 cycle (WR >= RAS-RCD and WR >= RC-RCD-RP)
                (0b0110UL << 12) | // RC = 60ns = 7 cycles  (p29)
                (0b0100UL << 8)  | // RAS = 42ns = 5 cycles (p29)
                (0b0111UL << 4)  | // XSR = 67ns = 8 cycles (p30)
                (0b0001UL << 0);   // MRD = 2 CK            (p30)
    }

    // Set MODE bits to 001 to start delivering the clock to the memory
    FMC_SDRAM->SDCMR = 0b001UL | FMC_SDCMR_CTB1;
    // Wait during the prescribed delay period
    chThdSleepMicroseconds(100);
    // Issue a PrechargeAll command
    FMC_SDRAM->SDCMR = 0b010UL | FMC_SDCMR_CTB1;
    // Configure number of consecutive Auto-refresh commands (NRFS) (at least 2)
    FMC_SDRAM->SDCMR = 0b011UL | FMC_SDCMR_CTB1 | (0b0001UL << 5);
    // Load Mode Register: BurstLength=1, BurstType=0 (don't care), CAS=3
    FMC_SDRAM->SDCMR = 0b100UL | FMC_SDCMR_CTB1 | (((0b011UL << 4) | 0b0000UL) << 9);
    // Program the refresh rate. Refresh Period for 8192 rows is 64ms.
    FMC_SDRAM->SDRTR = 292 << 1; // (64ms / 8192) * 40MHz - 20
    // ==== Remap SDRAM to 0x60000000 ====
#ifdef REMAP_TO_0x6ETC
    SYSCFG->MEMRMP |= SYSCFG_MEMRMP_SWP_FMC_0;
#endif
}

void SdramDeinit() {
    RCC->AHB3ENR &= ~RCC_AHB3ENR_FMCEN;
    // PortA
    PinSetupAnalog(GPIOA, 7); // SDNWE
    // PortC
    PinSetupAnalog(GPIOC, 4); // SDNE0
    PinSetupAnalog(GPIOC, 5); // SDCKE0
    // PortD
    PinSetupAnalog(GPIOD, 0); // D2
    PinSetupAnalog(GPIOD, 1);
    PinSetupAnalog(GPIOD, 8);
    PinSetupAnalog(GPIOD, 9);
    PinSetupAnalog(GPIOD, 10);
    PinSetupAnalog(GPIOD, 14);
    PinSetupAnalog(GPIOD, 15);
    // PortE
    PinSetupAnalog(GPIOE, 0);
    PinSetupAnalog(GPIOE, 1);
    PinSetupAnalog(GPIOE, 7);
    PinSetupAnalog(GPIOE, 8);
    PinSetupAnalog(GPIOE, 9);
    PinSetupAnalog(GPIOE, 10);
    PinSetupAnalog(GPIOE, 11);
    PinSetupAnalog(GPIOE, 12);
    PinSetupAnalog(GPIOE, 13);
    PinSetupAnalog(GPIOE, 14);
    PinSetupAnalog(GPIOE, 15);
    // PortF
    PinSetupAnalog(GPIOF, 0);
    PinSetupAnalog(GPIOF, 1);
    PinSetupAnalog(GPIOF, 2);
    PinSetupAnalog(GPIOF, 3);
    PinSetupAnalog(GPIOF, 4);
    PinSetupAnalog(GPIOF, 5);
    PinSetupAnalog(GPIOF, 11);
    PinSetupAnalog(GPIOF, 12);
    PinSetupAnalog(GPIOF, 13);
    PinSetupAnalog(GPIOF, 14);
    PinSetupAnalog(GPIOF, 15);
    // PortG
    PinSetupAnalog(GPIOG, 0);
    PinSetupAnalog(GPIOG, 1);
    PinSetupAnalog(GPIOG, 2);
    PinSetupAnalog(GPIOG, 4);
    PinSetupAnalog(GPIOG, 5);
    PinSetupAnalog(GPIOG, 8);
    PinSetupAnalog(GPIOG, 15);
}

void SdramCheck() {
    uint32_t ms;
    uint32_t Cnt = SDRAM_SZ_DWORDS;
    uint32_t Addr = SDRAM_ADDR;
    systime_t Start = chVTGetSystemTimeX();
    // Write
    for(uint32_t i=0; i<Cnt; i++) {
        *(volatile uint32_t*)Addr = i;
        Addr += 4;
    }
    ms = TIME_I2MS(chVTTimeElapsedSinceX(Start));
    Printf("Write: %u; %f MByte/s\r", ms, ((float)Cnt * 4 / 1000) / (float)ms );

//    Addr = 0xC0001000;
//    *(volatile uint32_t*)Addr = 0xCA115EA1UL;

    // Read
    Addr = SDRAM_ADDR;
    Start = chVTGetSystemTimeX();
    for(uint32_t i=0; i<Cnt; i++) {
        volatile uint32_t Value = *(volatile uint32_t*)Addr;
        Addr += 4;
        if(Value != i) Printf("%X\r", Value);
    }
    ms = TIME_I2MS(chVTTimeElapsedSinceX(Start));
    Printf("Read: %u; %f MByte/s\r", ms, ((float)Cnt * 4 / 1000) / (float)ms );
}

extern "C"
void* _sbrk(int incr) {
    extern uint8_t __heap_base__;
    extern uint8_t __heap_end__;

    static uint8_t *current_end = &__heap_base__;
    uint8_t *current_block_address = current_end;

    incr = (incr + 3) & (~3);
    if(current_end + incr > &__heap_end__) {
        errno = ENOMEM;
        return (void*) -1;
    }
    current_end += incr;
    return (void*)current_block_address;
}
