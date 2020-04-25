/*
 * Lora.cpp
 *
 *  Created on: 18 апр. 2020 г.
 *      Author: layst
 */

#include "Lora.h"
#include "sx1276.h"
#include "board.h"

Lora_t Lora;
SX1276_t sx;

void Lora_t::Init() {
    sx.Init();
}
