/*
 * lcdtft.h
 *
 *  Created on: 4 ����. 2019 �.
 *      Author: Kreyl
 */

#pragma once

void LcdInit();

void LcdDraw(uint32_t Left, uint32_t Top, uint32_t* Img, uint32_t ImgW, uint32_t ImgH);

void LcdPaintL1(uint32_t Left, uint32_t Top, uint32_t Right, uint32_t Bottom, uint32_t A, uint32_t R, uint32_t G, uint32_t B);
