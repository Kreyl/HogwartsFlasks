/*
 * lcdtft.h
 *
 *  Created on: 4 ����. 2019 �.
 *      Author: Kreyl
 */

#pragma once

#include "color.h"

// Display size
#define LCD_WIDTH       480UL
#define LCD_HEIGHT      272UL

#define LCD_PIXEL_SZ    3UL
#define LBUF_SZ         (LCD_PIXEL_SZ * LCD_WIDTH * LCD_HEIGHT)
#define LBUF_SZ32       (LBUF_SZ / 4)

#define LBUF_IN_SDRAM_MALLOC    FALSE
#define LBUF_IN_SDRAM_STATIC    TRUE

void LcdInit();
void LcdDeinit();

void LcdDrawARGB(uint32_t Left, uint32_t Top, uint32_t* Img, uint32_t ImgW, uint32_t ImgH);
void LcdDrawRGB(uint32_t Left, uint32_t Top, uint32_t* Img, uint32_t ImgW, uint32_t ImgH);

void LcdPaintL1(uint32_t Left, uint32_t Top, uint32_t Right, uint32_t Bottom, uint32_t A, uint32_t R, uint32_t G, uint32_t B);


uint8_t LcdDrawBmp(uint8_t *Buff, uint32_t Sz);

#if LBUF_IN_SDRAM_MALLOC
extern uint32_t *FrameBuf1;
#elif LBUF_IN_SDRAM_STATIC
extern uint32_t FrameBuf1[LBUF_SZ32];
#else
extern uint32_t FrameBuf1[LBUF_SZ32];
#endif

namespace Dma2d {
void Init();
void CopyBufferRGB(void *pSrc, uint32_t *pDst, uint32_t x, uint32_t y, uint32_t xsize, uint32_t ysize);
void Cls(uint32_t *pDst, uint8_t R=0, uint8_t G=0, uint8_t B=0);

void WaitCompletion();
void Suspend();
void Resume();
}; // namespace
