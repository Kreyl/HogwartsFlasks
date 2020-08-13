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

void LcdInit();

void LcdDrawARGB(uint32_t Left, uint32_t Top, uint32_t* Img, uint32_t ImgW, uint32_t ImgH);
void LcdDrawRGB(uint32_t Left, uint32_t Top, uint32_t* Img, uint32_t ImgW, uint32_t ImgH);

void LcdPaintL1(uint32_t Left, uint32_t Top, uint32_t Right, uint32_t Bottom, uint32_t A, uint32_t R, uint32_t G, uint32_t B);


uint8_t LcdDrawBmp(uint8_t *Buff, uint32_t Sz);

extern ColorARGB_t *FrameBuf1;
