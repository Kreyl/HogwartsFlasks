/*
 * AviDecode.h
 *
 *  Created on: 8 авг. 2020 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>

namespace Avi {

void Init();
uint8_t Start(const char* FName);
void Stop();

}
