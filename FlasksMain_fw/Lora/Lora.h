/*
 * Radio.h
 *
 *  Created on: 18 апр. 2020 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>
#include "kl_lib.h"

#define SX_MAX_BAUDRATE_HZ  10000000

class Lora_t {
private:
//    const GPIO_TypeDef *DIO0Gpio, *DIO1Gpio, *DIO2Gpio, *DIO3Gpio;
//    const uint16_t DIO0Pin, DIO1Pin, DIO2Pin, DIO3Pin;
//    const PinIrq_t IGdo0;
    void WriteReg(uint8_t RegAddr, uint8_t Value);
    uint8_t ReadReg (uint8_t RegAddr);
    // Middle level
    void RxChainCalibration();
    void SetChannel(uint32_t freq);
    void SetOpMode(uint8_t opMode);
public:
    uint32_t Channel;
    uint8_t Init();
};

extern Lora_t Lora;
