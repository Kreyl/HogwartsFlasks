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



//union BufClr_t {
//    uint32_t Word32;
//    struct {
//        uint8_t A;
//        uint8_
//    };
//};

#define LBUF_CNT32      8192
#define LBUF_SZ         (LBUF_CNT32 * 4)
//uint32_t FrameBuf1[LBUF_CNT32];
uint32_t FrameBuf2[LBUF_CNT32];

volatile uint32_t *FrameBuf1 = (volatile uint32_t*)SDRAM_ADDR;

struct RGB_t {
    uint8_t R, G, B;
} __attribute__((packed));

void LcdInit() {
    Backlight.Init();
    Backlight.SetFrequencyHz(10000);
    Backlight.Set(100);

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
//    LTDC->BCCR = (0x55UL << 16) | (0xAAUL << 8) | 0x55UL;
//    LTDC->BCCR = (0xFUL << 16) | (0xFFUL << 8) | 0xFFUL;
    LTDC->BCCR = 0; // Paint it black

    // === Layer 1 ===
    // layer window horizontal and vertical position
    LTDC_Layer1->WHPCR = (135UL << 16) | 45UL; // Stop and Start positions
    LTDC_Layer1->WVPCR = (72UL << 16) | 36UL; // Stop and Start positions
//    LTDC_Layer1->PFCR  = 0b001UL;    // RGB888
    LTDC_Layer1->PFCR  = 0b000UL;    // ARGB8888
    LTDC_Layer1->CFBAR = (uint32_t)FrameBuf1; // Address of layer buffer
    LTDC_Layer1->CFBLR = (300UL << 16) | (300UL + 3UL); // 300 bytes per line
    LTDC_Layer1->CFBLNR = 100; // 100 lines in a buffer
//    LTDC_Layer1->DCCR = 0xFF55AA55; // Default color
    LTDC_Layer1->CR = 1;    // Enable layer

    // === Layer 2 ===
    // layer window horizontal and vertical position
    LTDC_Layer2->WHPCR = (108UL << 16) | 72UL; // Stop and Start positions
    LTDC_Layer2->WVPCR = (135UL << 16) | 108UL; // Stop and Start positions
//    LTDC_Layer2->PFCR  = 0b001UL;    // RGB888
    LTDC_Layer2->PFCR  = 0b000UL;    // ARGB8888
    LTDC_Layer2->CFBAR = (uint32_t)FrameBuf2; // Address of layer buffer
    LTDC_Layer2->CFBLR = (300UL << 16) | (300UL + 3UL); // 300 bytes per line
    LTDC_Layer2->CFBLNR = 100; // 100 lines in a buffer
//    LTDC_Layer2->DCCR = 0xFF55AA55; // Default color
    LTDC_Layer2->CR = 1; // Enable layer

    // Immediately reload new values from shadow regs
    LTDC->SRCR = 1;
    // En LCD controller
    LTDC->GCR |= 1;

    // Fill Layer 1
    for(uint32_t i=0; i<LBUF_CNT32; i++) {
        FrameBuf1[i] = 0xFFFFFFFF;
        FrameBuf2[i] = 0xFFFFFFFF;
    }


    // Enable display
    PinSetupOut(LCD_DISP, omPushPull);
    PinSetHi(LCD_DISP);
}

void LcdPaintL1(uint32_t Left, uint32_t Top, uint32_t Right, uint32_t Bottom, uint32_t A, uint32_t R, uint32_t G, uint32_t B) {
    uint32_t v = (A << 24) | (R << 16) | (G << 8) | (B << 0);
//    Printf("L%u T%u; R%u B%u; %u %u %u %u;   %X\r", Left, Top, Right, Bottom, A, R,G,B, v);
    LTDC_Layer1->WHPCR = (Right << 16) | Left; // Stop and Start positions
    LTDC_Layer1->WVPCR = (Bottom << 16) | Top; // Stop and Start positions
    LTDC->SRCR = 1; // Immediately reload new values from shadow regs

    for(uint32_t i=0; i<LBUF_CNT32; i++) {
        FrameBuf1[i] = v;
    }
}

