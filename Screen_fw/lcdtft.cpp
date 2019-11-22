/*
 * lcdtft.cpp
 *
 *  Created on: 4 но€б. 2019 г.
 *      Author: Kreyl
 */

#include "board.h"
#include "kl_lib.h"
#include "shell.h"
#include "sdram.h"
#include "color.h"

// Display size
#define LCD_WIDTH       480UL
#define LCD_HEIGHT      272UL

// Sync params
#define HSYNC_WIDTH     4UL  // See display datasheet
#define HBACK_PORCH     43UL // See display datasheet
#define HBP             (HBACK_PORCH - HSYNC_WIDTH) // horizontal back porch. In case of HDPOL=1 (see display ds p61) HBP starts togeteher with HSYNC, i.e. contains it (HSYNC + HBP).
#define ACTIVE_WIDTH    LCD_WIDTH
#define HFP             8UL // horizontal front porch

#define VSYNC_WIDTH     4UL  // See display datasheet
#define VBACK_PORCH     12UL // See display datasheet
#define VBP             (VBACK_PORCH - VSYNC_WIDTH) // vertical back porch. In case of VDPOL=1 (see display ds p61) VBP starts togeteher with HSYNC, i.e. contains it (VSYNC + VBP).
#define ACTIVE_HEIGHT   LCD_HEIGHT
#define VFP             8UL // vertical front porch

PinOutputPWM_t Backlight{LCD_BCKLT};

// Layer Buffers and other sizes
#define LBUF_SZ         (sizeof(Color_t) * LCD_WIDTH * LCD_HEIGHT)
#define LBUF_CNT        (LCD_WIDTH * LCD_HEIGHT)
#define LINE_SZ         (sizeof(Color_t) * LCD_WIDTH)
ColorARGB_t *FrameBuf1 = (ColorARGB_t*)(SDRAM_ADDR + 0);
#define ENABLE_LAYER2   TRUE
#if ENABLE_LAYER2
ColorARGB_t *FrameBuf2 = (ColorARGB_t*)(SDRAM_ADDR + LBUF_SZ);
#endif

struct RGB_t {
    uint8_t R, G, B;
} __attribute__((packed));

void LcdInit() {
    Backlight.Init();
    Backlight.SetFrequencyHz(10000);
    Backlight.Set(100);

    // Enable clock. Pixel clock set up at main; <=12MHz according to display datasheet
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
    LTDC->BCCR = 0; // Paint it black

    // === Layer 1 ===
    memset(FrameBuf1, 0, LBUF_SZ); // Fill Layer 1
    // layer window horizontal and vertical position
    LTDC_Layer1->WHPCR = ((LCD_WIDTH  + HBACK_PORCH) << 16) | HBACK_PORCH; // Stop and Start positions
    LTDC_Layer1->WVPCR = ((LCD_HEIGHT + VBACK_PORCH) << 16) | VBACK_PORCH; // Stop and Start positions
//    LTDC_Layer1->PFCR  = 0b001UL;    // RGB888
    LTDC_Layer1->PFCR  = 0b000UL;    // ARGB8888
    LTDC_Layer1->CFBAR = (uint32_t)FrameBuf1; // Address of layer buffer
    LTDC_Layer1->CFBLR = (LINE_SZ << 16) | (LINE_SZ + 3UL); // 300 bytes per line
    LTDC_Layer1->CFBLNR = LCD_HEIGHT; // LCD_HEIGHT lines in a buffer
    LTDC_Layer1->CR = 1;    // Enable layer

#if ENABLE_LAYER2 // === Layer 2 ===
    LTDC_Layer2->WHPCR = (108UL << 16) | 72UL; // Stop and Start positions
    LTDC_Layer2->WVPCR = (135UL << 16) | 108UL; // Stop and Start positions
//    LTDC_Layer2->PFCR  = 0b001UL;    // RGB888
    LTDC_Layer2->PFCR  = 0b000UL;    // ARGB8888
    LTDC_Layer2->CFBAR = (uint32_t)FrameBuf2; // Address of layer buffer
    LTDC_Layer2->CFBLR = (300UL << 16) | (300UL + 3UL); // 300 bytes per line
    LTDC_Layer2->CFBLNR = 100; // 100 lines in a buffer
    LTDC_Layer2->CR = 1; // Enable layer
#endif

    LTDC->SRCR = 1; // Immediately reload new values from shadow regs
    LTDC->GCR |= 1; // En LCD controller

    // Enable display
    PinSetupOut(LCD_DISP, omPushPull);
    PinSetHi(LCD_DISP);
}

void LcdPaintL1(uint32_t Left, uint32_t Top, uint32_t Right, uint32_t Bottom, uint32_t A, uint32_t R, uint32_t G, uint32_t B) {
//    Printf("L%u T%u; R%u B%u; %u %u %u %u;   %X\r", Left, Top, Right, Bottom, A, R,G,B, v);
//    ColorARGB_t *ptr = FrameBuf1 +


//    for(uint32_t i=0; i<LBUF_CNT; i++) {
//        FrameBuf1[i].DWord32 = v;
//    }
}

