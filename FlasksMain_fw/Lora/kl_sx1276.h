/*
 * sx1276.h
 *
 *  Created on: 18 апр. 2020 г.
 *      Author: layst
 */

#pragma once

#include "board.h"
#include "ch.h"
#include "hal.h"
#include "kl_lib.h"


class SX1276KL_t {
private:
    // Low level
    void NssHi() { PinSetHi(LORA_NSS); }
    void NssLo() { PinSetLo(LORA_NSS); }
    // Mid level
public:
    uint8_t Init();
};
