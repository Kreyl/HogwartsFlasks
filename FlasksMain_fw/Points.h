/*
 * Points.h
 *
 *  Created on: 4 мая 2021 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>

#define INDX_GRIF           0
#define INDX_SLYZE          1
#define INDX_RAVE           2
#define INDX_HUFF           3

namespace Points {

void Init();
void Set(int32_t AGrif, int32_t ASlyze, int32_t ARave, int32_t AHuff);

void Print();

} // namespace
