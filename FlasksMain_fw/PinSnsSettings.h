/*
 * SnsPins.h
 *
 *  Created on: 17 ���. 2015 �.
 *      Author: Kreyl
 */

/* ================ Documentation =================
 * There are several (may be 1) groups of sensors (say, buttons and USB connection).
 *
 */

#pragma once

#include "SimpleSensors.h"
#include "kl_lib.h"

#ifndef SIMPLESENSORS_ENABLED
#define SIMPLESENSORS_ENABLED   FALSE
#endif

#if SIMPLESENSORS_ENABLED
#define SNS_POLL_PERIOD_MS      63

// Handlers
extern void ProcessButtons(PinSnsState_t *PState, uint32_t Len);

const PinSns_t PinSns[] = {
        // Buttons
        {GPIOB, 13, pudPullUp, ProcessButtons},
        {GPIOC,  2, pudPullUp, ProcessButtons},
        {GPIOA, 12, pudPullUp, ProcessButtons},
        {GPIOA,  9, pudPullUp, ProcessButtons},
        {GPIOA, 11, pudPullUp, ProcessButtons},

        {GPIOD,  2, pudPullUp, ProcessButtons},
        {GPIOC,  8, pudPullUp, ProcessButtons},
        {GPIOC, 12, pudPullUp, ProcessButtons},



};
#define PIN_SNS_CNT     countof(PinSns)

#endif  // if enabled
