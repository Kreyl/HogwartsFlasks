/*
 * AviDecode.h
 *
 *  Created on: 8 авг. 2020 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>

#define AVI_AUDIO_EN    FALSE

namespace Avi {

void Init();
void Standby();
void Resume();
uint8_t Start(const char* FName, uint32_t FrameN = 0);
void Stop();

}
