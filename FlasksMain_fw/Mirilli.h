/*
 * Mirilli.h
 *
 *  Created on: 30 сент. 2017 г.
 *      Author: Kreyl
 */

#ifndef MIRILLI_H__
#define MIRILLI_H__

#include "color.h"

#define MIRILLI_USE_WS2812
//#define MIRILLI_USE_SK6812

#define SMOOTH_VALUE    360
#define OFF_LUMINOCITY  1       // Set to 0 if "backlight all" is not required

#define MIRILLI_H_CNT   13
#define MIRILLI_M_CNT   12

// Tables of accordance between hours/minutes and LED indxs. Hours and hyperminutes are 12
static const uint32_t H2LedN[12] = { 12, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14 };
#define SECOND_0_LED_INDX   13
static const uint32_t M2LedN[12] = { 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

// Do not touch
#define MIRILLI_LED_CNT         (MIRILLI_H_CNT + MIRILLI_M_CNT)   // Number of WS2812 LEDs

void InitMirilli();
void SetTargetClrH(uint32_t H, Color_t Clr);
void SetTargetClrM(uint32_t M, Color_t Clr);
void WakeMirilli();
void ResetColorsToOffState(Color_t ClrH, Color_t ClrM);

#endif //MIRILLI_H__