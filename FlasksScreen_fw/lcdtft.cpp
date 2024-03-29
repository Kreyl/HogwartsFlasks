/*
 * lcdtft.cpp
 *
 *  Created on: 4 ����. 2019 �.
 *      Author: Kreyl
 */

#include "board.h"
#include "kl_lib.h"
#include "shell.h"
#include "sdram.h"
#include "color.h"
#include "lcdtft.h"

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
#define LBUF_CNT        (LCD_WIDTH * LCD_HEIGHT)
#define LINE_SZ         (LCD_PIXEL_SZ * LCD_WIDTH)

#if LBUF_IN_SDRAM_MALLOC
uint32_t *FrameBuf1;
#elif LBUF_IN_SDRAM_STATIC
__attribute__ ((section (".static_sdram")))
uint32_t FrameBuf1[LBUF_SZ32];
#else
__attribute__ ((section ("DATA_RAM")))
uint32_t FrameBuf1[LBUF_SZ32];
#endif

//__attribute__ ((section (".static_sdram")))
//uint8_t SFBuf[FBUF_SZ];


//#define ENABLE_LAYER2   TRUE
#if ENABLE_LAYER2
ColorRGB_t *FrameBuf2;
#endif

const stm32_dma_stream_t *PDmaMCpy;

void LcdInit() {
    Backlight.Init();
    Backlight.SetFrequencyHz(10000);
    Backlight.Set(100);

#if LBUF_IN_SDRAM_MALLOC
    FrameBuf1 = (uint32_t*)malloc(LBUF_SZ);
#endif

#if ENABLE_LAYER2
    FrameBuf2 = (ColorRGB_t*)malloc(LBUF_SZ);
#endif

    // Enable clock. Pixel clock set up at main; <=12MHz according to display datasheet
    RCC->APB2RSTR |= RCC_APB2RSTR_LTDCRST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_LTDCRST;
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

#if 1 // ==== HSYNC / VSYNC etc. ====
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
#endif

#if 1 // === Layer 1 ===
//    memset(FrameBuf1, 0, LBUF_SZ); // Fill Layer 1
    // layer window horizontal and vertical position
    LTDC_Layer1->WHPCR = ((LCD_WIDTH  + HBACK_PORCH - 1UL) << 16) | HBACK_PORCH; // Stop and Start positions
    LTDC_Layer1->WVPCR = ((LCD_HEIGHT + VBACK_PORCH - 1UL) << 16) | VBACK_PORCH; // Stop and Start positions
    // Color format
#if LCD_PIXEL_SZ == 2
    LTDC_Layer1->PFCR  = 0b010UL;    // RGB565
#elif LCD_PIXEL_SZ == 3
    LTDC_Layer1->PFCR  = 0b001UL;    // RGB888
#elif LCD_PIXEL_SZ == 4
    LTDC_Layer1->PFCR  = 0b000UL;    // ARGB8888
#endif
    LTDC_Layer1->CFBAR = (uint32_t)FrameBuf1; // Address of layer buffer
    LTDC_Layer1->CFBLR = (LINE_SZ << 16) | (LINE_SZ + 3UL);
    LTDC_Layer1->CFBLNR = LCD_HEIGHT; // LCD_HEIGHT lines in a buffer
    LTDC_Layer1->CR = 1;    // Enable layer
#endif

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

void LcdDeinit() {
    Backlight.Set(0);
    RCC->APB2ENR &= ~RCC_APB2ENR_LTDCEN;
    PinSetLo(LCD_DISP);

#if 1 // ==== GPIO ====
    // GpioA
    PinSetupAnalog(GPIOA, 1);
    PinSetupAnalog(GPIOA, 2);
    PinSetupAnalog(GPIOA, 3);
    PinSetupAnalog(GPIOA, 4);
    PinSetupAnalog(GPIOA, 5);
    PinSetupAnalog(GPIOA, 6);
    PinSetupAnalog(GPIOA, 10);
    // GpioB
    PinSetupAnalog(GPIOB, 0);
    PinSetupAnalog(GPIOB, 1);
    PinSetupAnalog(GPIOB, 8);
    PinSetupAnalog(GPIOB, 9);
    PinSetupAnalog(GPIOB, 10);
    PinSetupAnalog(GPIOB, 11);
    // GpioC
    PinSetupAnalog(GPIOC, 0);
    PinSetupAnalog(GPIOC, 6);
    PinSetupAnalog(GPIOC, 7);
    PinSetupAnalog(GPIOC, 9);
    // GpioD
    PinSetupAnalog(GPIOD, 3);
    // GpioE
    PinSetupAnalog(GPIOE, 4);
    PinSetupAnalog(GPIOE, 5);
    PinSetupAnalog(GPIOE, 6);
    // GpioG
    PinSetupAnalog(GPIOG, 6);
    PinSetupAnalog(GPIOG, 7);
    PinSetupAnalog(GPIOG, 10);
    PinSetupAnalog(GPIOG, 11);
    PinSetupAnalog(GPIOG, 12);
    PinSetupAnalog(GPIOG, 13);
#endif
}


void LcdDrawARGB(uint32_t Left, uint32_t Top, uint32_t* Img, uint32_t ImgW, uint32_t ImgH) {
    uint32_t *dst;
    for(uint32_t y = 0; y<ImgH; y++) {
        dst = (uint32_t*)(FrameBuf1 + Left + ((Top + y) * LCD_WIDTH));
        for(uint32_t x=0; x<ImgW; x++) {
            if((Left + x) < LCD_WIDTH and (Top + y) < LCD_HEIGHT) *dst++ = *Img;
            Img++;
        }
    }
}

void LcdPaintL1(uint32_t Left, uint32_t Top, uint32_t Right, uint32_t Bottom, uint32_t A, uint32_t R, uint32_t G, uint32_t B) {
    volatile uint32_t v = (R << 16) | (G << 8) | B;

//    for(uint32_t i=0; i<LBUF_CNT; i++) {
//        FrameBuf1[i].DWord32 = v;
//    }

    dmaStreamSetPeripheral(PDmaMCpy, &v);
    dmaStreamSetMemory0(PDmaMCpy, FrameBuf1);
    dmaStreamSetTransactionSize(PDmaMCpy, 0xFFFF);
    dmaStreamSetMode(PDmaMCpy, (DMA_PRIORITY_HIGH | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2M));
    dmaStreamEnable(PDmaMCpy);
    dmaWaitCompletion(PDmaMCpy);

//    dmaStreamSetMemory0(PDmaMCpy, FrameBuf1 + 0xFFFFUL);
//    dmaStreamSetTransactionSize(PDmaMCpy, LBUF_CNT- 0xFFFFUL);
//    dmaStreamEnable(PDmaMCpy);
//    dmaWaitCompletion(PDmaMCpy);
//
//
//    for(uint32_t i=0; i<LBUF_CNT; i++) {
//        if(FrameBuf1[i].DWord32 != v) Printf("%d %X\r", i, FrameBuf1[i].DWord32);
//        (void)FrameBuf1[i];
//    }
}


#define BITMAP_SIGNATURE 0x4d42

struct BmpFileHeader_t {
    uint16_t Signature;
    uint32_t Size;
    uint32_t Reserved;
    uint32_t BitsOffset;
} __attribute__((packed));

struct BmpHeader_t {
    uint32_t HeaderSize;
    int32_t Width;
    int32_t Height;
    uint16_t Planes;
    uint16_t BitCount;
    uint32_t Compression;
    uint32_t SizeImage;
    int32_t PelsPerMeterX;
    int32_t PelsPerMeterY;
    uint32_t ClrUsed;
    uint32_t ClrImportant;
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
    uint32_t AlphaMask;
    uint32_t CsType;
    uint32_t Endpoints[9]; // see http://msdn2.microsoft.com/en-us/library/ms536569.aspx
    uint32_t GammaRed;
    uint32_t GammaGreen;
    uint32_t GammaBlue;
} __attribute__((packed));


uint8_t LcdDrawBmp(uint8_t *Buff, uint32_t Sz) {
    // Stop DMA2D
    if(DMA2D->CR & DMA2D_CR_START) {
        DMA2D->CR |= DMA2D_CR_ABORT;
        while(DMA2D->CR & DMA2D_CR_START);
    }
    DMA2D->CR &= ~DMA2D_CR_MODE; // Mem 2 Mem, Foreground only

//    DMA2D->CR |= DMA2D_CR_MODE; // Reg 2 mem
    DMA2D->OPFCCR = 0; // Output: no RedBluSwap, no Alpha inv, color mode = ARGB8888
    DMA2D->OMAR = (uint32_t)FrameBuf1; // Output address
    DMA2D->OOR = 0; // no output offset
//    DMA2D->OCOLR = 0xFFFFFFFF;
    DMA2D->NLR = (480UL << 16) | 234UL;
//    DMA2D->CR |= DMA2D_CR_START;
//    while(DMA2D->CR & DMA2D_CR_START);
//    Printf("CR %X; ISR %X; OMAR %X\r", DMA2D->CR, DMA2D->ISR, DMA2D->OMAR);
//    return retvOk;

    // Setup foreground pixel converter: Alpha=0xFF and replaces original
    DMA2D->FGPFCCR = (0xFFUL << 24) | (0b01UL << 16);
    DMA2D->FGOR = 0; // no foreground offset
    // Read headers
    uint8_t *Ptr = Buff;
    BmpFileHeader_t BmpFileHeader;
    memcpy(&BmpFileHeader, Ptr, sizeof(BmpFileHeader_t));
    if(BmpFileHeader.Signature != BITMAP_SIGNATURE) return retvFail;
    Ptr += sizeof(BmpFileHeader_t);
    BmpHeader_t BmpHdr;
    memcpy(&BmpHdr, Ptr, sizeof(BmpHeader_t));
    // Setup Height & Width
    uint32_t Width  = (BmpHdr.Width < 0) ? -BmpHdr.Width : BmpHdr.Width;;
    uint32_t Height = (BmpHdr.Height < 0) ? -BmpHdr.Height : BmpHdr.Height;;
    DMA2D->NLR = (BmpHdr.Width << 16) | BmpHdr.Height;

    // Load Color Table
    if(BmpHdr.BitCount) {
//        Ptr = Buff + sizeof(BmpFileHeader_t) + BmpHdr.HeaderSize;
        if(BmpHdr.BitCount == 8) {
            DMA2D->FGPFCCR |= (255UL << 8) | (0b0101UL); // CLUT sz = 256, CLU color mode ARGB888, color mode = L8
            uint32_t Table[256];
            memcpy(Table, (Buff + sizeof(BmpFileHeader_t) + BmpHdr.HeaderSize), 1024);
            DMA2D->FGCMAR = (uint32_t)Table; // Address to load CLUT from
            DMA2D->FGPFCCR |= DMA2D_FGPFCCR_START;
            while(DMA2D->FGPFCCR & DMA2D_FGPFCCR_START);

            Printf("CLUT CR %X; ISR %X; FGPFCCR %X; FGCMAR %X\r", DMA2D->CR, DMA2D->ISR, DMA2D->FGPFCCR, DMA2D->FGCMAR);
        }
    }

    // Read pixels
    Ptr = Buff + BmpFileHeader.BitsOffset; // Start of pixel data
    uint32_t Index = 0;

    if(BmpHdr.Compression == 0) {

    }
    else if(BmpHdr.Compression == 1) { // RLE 8
        uint32_t Count = 0;
        uint32_t ColorIndex = 0;
        uint32_t x = 0, y = 0;
        uint8_t *Pixels = (uint8_t*)malloc(Width * Height);

        while(Ptr < (Buff + Sz)) {
            Count = *Ptr++;
            ColorIndex = *Ptr++;

            if(Count > 0) {
                Index = x + y * Width;
                memset(&Pixels[Index], ColorIndex, Count);
                x += Count;
            }
            else { // Count == 0
                uint32_t Flag = ColorIndex;
                if(Flag == 0) {
                    x = 0;
                    y++;
                }
                else if(Flag == 1) break;
                else if(Flag == 2) {
                    x += *Ptr++; // rx
                    y += *Ptr++; // ry
                }
                else {
                    Count = Flag;
                    Index = x + y * Width;
                    memset(&Pixels[Index], ColorIndex, Count);
                    x += Count;
                    uint32_t pos = Ptr - Buff;
                    if(pos & 1) Ptr++;
                }
            }
        } // while
        // Indexed colors bitmap filled
        DMA2D->FGMAR = (uint32_t)Pixels;
        DMA2D->CR |= DMA2D_CR_START;
        while(DMA2D->CR & DMA2D_CR_START);
        Printf("CR %X; ISR %X; OMAR %X\r", DMA2D->CR, DMA2D->ISR, DMA2D->OMAR);
    } // if compression

    return retvOk;
}

#if 1 // =============================== DMA2D =================================
// When the DMA2D operates in memory-to-memory operation with pixel format conversion (no blending operation), the BG FIFO is not activated
namespace Dma2d {
void Init() {
    rccEnableDMA2D(FALSE);
    DMA2D->AMTCR = (64UL << 8) | 1UL;
    DMA2D->FGOR = 0; // No offset
#if LCD_PIXEL_SZ == 3
    DMA2D->OPFCCR = DMA2D_OPFCCR_RBS | 0b001UL; // Red Blue swap, no alpha inversion, RGB888
#elif LCD_PIXEL_SZ == 4
    DMA2D->OPFCCR = 0; // No Red Blue swap, no alpha inversion, ARGB8888
#endif
}

void CopyBufferRGB(void *pSrc, uint32_t *pDst, uint32_t x, uint32_t y, uint32_t xsize, uint32_t ysize) {
    DMA2D->CR = (0b01UL << 16); // Mode PFC
    // Alpha = 0xFF, no RedBlueSwap, no Alpha Inversion, replace Alpha, RGB888
    DMA2D->FGPFCCR = 0b0001UL;
//    Printf("S %X\r", pSrc);
    DMA2D->FGMAR = (uint32_t)pSrc; // Input data
    // Output
    DMA2D->OMAR = (uint32_t)pDst + (y * LCD_WIDTH + x) * LCD_PIXEL_SZ;
    DMA2D->OOR = LCD_WIDTH - xsize; // Output offset
    DMA2D->NLR = (xsize << 16) | ysize; // Output PixelPerLine and Number of Lines
    // Start
    DMA2D->IFCR = 0x3FUL; // Clear all flags
    DMA2D->CR |= DMA2D_CR_START;
}

void Cls(uint32_t *pDst, uint8_t R, uint8_t G, uint8_t B) {
    DMA2D->CR = (0b11UL << 16); // Mode Reg-to-Mem
    // Set color
    DMA2D->OCOLR = (0xFFUL << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) |((uint32_t)B << 0);
    // Output
    DMA2D->OMAR = (uint32_t)pDst;
    DMA2D->OOR = 0; // Output offset
    DMA2D->NLR = (LCD_WIDTH << 16) | LCD_HEIGHT; // Output PixelPerLine and Number of Lines
    // Start
    DMA2D->IFCR = 0x3FUL; // Clear all flags
    DMA2D->CR |= DMA2D_CR_START;
}

void WaitCompletion() { while(!(DMA2D->ISR & DMA2D_ISR_TCIF)); }
void Suspend() { DMA2D->CR |=  DMA2D_CR_SUSP; }
void Resume()  { DMA2D->CR &= ~DMA2D_CR_SUSP; }
}; // namespace
#endif
