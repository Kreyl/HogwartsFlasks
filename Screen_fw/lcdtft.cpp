/*
 * lcdtft.cpp
 *
 *  Created on: 4 но€б. 2019 г.
 *      Author: Kreyl
 */

#include "board.h"
#include "kl_lib.h"

// horizontal synchronization width (in units of pixel clock period)
#define HSW
// vertical synchronization height (in units of horizontal scan line)
#define VSH

#define TOTAL_WIDTH
#define HSYNC_WIDTH     4UL
#define HBP             (43UL - HSYNC_WIDTH) // horizontal back porch. In case of HDPOL=1 (see display ds p61) HBP starts togeteher with HSYNC, i.e. contains it (HSYNC + HBP).
#define ACTIVE_WIDTH    480UL
#define HFP             8UL // horizontal front porch

#define TOTAL_HEIGHT
#define VSYNC_WIDTH     4UL
#define VBP             (12UL - VSYNC_WIDTH) // vertical back porch. In case of VDPOL=1 (see display ds p61) VBP starts togeteher with HSYNC, i.e. contains it (VSYNC + VBP).
#define ACTIVE_HEIGHT   272UL
#define VFP             8UL // vertical front porch


PinOutputPWM_t Backlight{LCD_BCKLT};

void LcdInit() {
//    Backlight.Init();
//    Backlight.SetFrequencyHz(10000);
//    Backlight.Set(100);

    // Enable clock
    RCC->APB2ENR |= RCC_APB2ENR_LTDCEN;

#if 1 // ==== GPIO ====
    // GpioA
    PinSetupAlterFunc(GPIOA, 1, omPushPull, pudNone, AF14); // R2
    PinSetupAlterFunc(GPIOA, 2, omPushPull, pudNone, AF14); // R1
    PinSetupAlterFunc(GPIOA, 3, omPushPull, pudNone, AF14); // B5
    PinSetupAlterFunc(GPIOA, 4, omPushPull, pudNone, AF14); // VSYNC
    PinSetupAlterFunc(GPIOA, 5, omPushPull, pudNone, AF14); // R4
    PinSetupAlterFunc(GPIOA, 6, omPushPull, pudNone, AF14); // G2
    PinSetupAlterFunc(GPIOA, 10, omPushPull, pudNone, AF9); // B4
    // GpioB
    PinSetupAlterFunc(GPIOB, 0, omPushPull, pudNone, AF9); // R3
    PinSetupAlterFunc(GPIOB, 1, omPushPull, pudNone, AF9); // R6
    PinSetupAlterFunc(GPIOB, 8, omPushPull, pudNone, AF14); // B6
    PinSetupAlterFunc(GPIOB, 9, omPushPull, pudNone, AF14); // B7
    PinSetupAlterFunc(GPIOB, 10, omPushPull, pudNone, AF14); // G4
    PinSetupAlterFunc(GPIOB, 11, omPushPull, pudNone, AF14); // G5
    // GpioC
    PinSetupAlterFunc(GPIOC, 0, omPushPull, pudNone, AF14); // R5
    PinSetupAlterFunc(GPIOC, 6, omPushPull, pudNone, AF14); // HSYNC
    PinSetupAlterFunc(GPIOC, 7, omPushPull, pudNone, AF14); // G6
    PinSetupAlterFunc(GPIOC, 9, omPushPull, pudNone, AF10); // G3
    // GpioD
    PinSetupAlterFunc(GPIOD, 3, omPushPull, pudNone, AF14); // G7
    // GpioE
    PinSetupAlterFunc(GPIOE, 4, omPushPull, pudNone, AF14); // B0
    PinSetupAlterFunc(GPIOE, 5, omPushPull, pudNone, AF14); // G0
    PinSetupAlterFunc(GPIOE, 6, omPushPull, pudNone, AF14); // G1
    // GpioF
//    PinSetupAlterFunc(GPIOF, 10, omPushPull, pudNone, AF14); // DE
    // Disable DE, tie it to 0
    PinSetupOut(GPIOF, 10, omPushPull);
    PinSetLo(GPIOF, 10);
    // GpioG
    PinSetupAlterFunc(GPIOG, 6, omPushPull, pudNone, AF14); // R7
    PinSetupAlterFunc(GPIOG, 7, omPushPull, pudNone, AF14); // CLK
    PinSetupAlterFunc(GPIOG, 10, omPushPull, pudNone, AF14); // B2
    PinSetupAlterFunc(GPIOG, 11, omPushPull, pudNone, AF14); // B3
    PinSetupAlterFunc(GPIOG, 12, omPushPull, pudNone, AF14); // B1
    PinSetupAlterFunc(GPIOG, 13, omPushPull, pudNone, AF14); // R0
#endif
    /* Pixel clock set up at main
     Configure the synchronous timings: VSYNC, HSYNC, vertical and horizontal back
porch, active data area and the front porch timings  */
    // HSYNC and VSYNC pulse width
    LTDC->SSCR = ((HSYNC_WIDTH - 1UL) << 16) | (VSYNC_WIDTH - 1UL);
    // HBP and VBP
    LTDC->BPCR = ((HSYNC_WIDTH + HBP - 1UL) << 16) | (VSYNC_WIDTH + VBP - 1UL);
    // Active width and active height
    LTDC->AWCR = ((HSYNC_WIDTH + HBP + ACTIVE_WIDTH - 1UL) << 16) | (VSYNC_WIDTH + VBP + ACTIVE_HEIGHT - 1UL);
    // Total width and height
    LTDC->TWCR = ((HSYNC_WIDTH + HBP + ACTIVE_WIDTH + HFP - 1UL) << 16) | (VSYNC_WIDTH + VBP + ACTIVE_HEIGHT + VFP - 1UL);
    // Sync signals and clock polarity
    LTDC->GCR =
            (0UL << 31) | // HSYNC is active low
            (0UL << 30) | // VSYNC is active low
            (1UL << 29) | // DE is active high
            (0UL << 28) | // CLK is active low
            (0UL << 16) | // Dither disable
            (0UL << 0);   // LTDC dis
    // Background color: R<<16 | G<<8 | B<<0
    LTDC->BCCR = (0x55UL << 16) | (0xAAUL << 8) | 0x55UL;

    // === Layer 1 ===
    // layer window horizontal and vertical position
    LTDC_Layer1->WHPCR =
    LTDC_Layer1->WVPCR =


    // Enable display
    PinSetupOut(LCD_DISP, omPushPull);
    PinSetHi(LCD_DISP);

}


