/*
 * Radio.h
 *
 *  Created on: 18 апр. 2020 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>
#include "kl_lib.h"
#include "sx1276Regs-Fsk.h"
#include "sx1276Regs-LoRa.h"

#define SX_MAX_BAUDRATE_HZ  10000000
#define SX_RF_OUT_PABOOST   TRUE  // Set to TRUE if RF out is PA_BOOST, and to FALSE if RFO_HF

// When using LoRa modem only bandwidths 125, 250 and 500 kHz are supported
enum SXLoraBW_t : uint8_t {
    bwLora125kHz = 0b0111,
    bwLora250kHz = 0b1000,
    bwLora500kHz = 0b1001
};

enum SXCodingRate_t : uint8_t {
    coderate4s5 = 0b001,
    coderate4s6 = 0b010,
    coderate4s7 = 0b011,
    coderate4s8 = 0b100
};

enum SXSpreadingFactor_t : uint8_t {
    sprfact64chipsPersym = 6,
    sprfact128chipsPersym = 7,
    sprfact256chipsPersym = 8,
    sprfact512chipsPersym = 9,
    sprfact1024chipsPersym = 10,
    sprfact2048chipsPersym = 11,
    sprfact4096chipsPersym = 12
};

void SX_DIO0IrqHandler();
void SX_DIO1IrqHandler();

class Lora_t {
private:
    const PinIrq_t DIO0{SX_DIO0_GPIO, SX_DIO0_PIN, pudNone, SX_DIO0IrqHandler};
    const PinIrq_t DIO1{SX_DIO1_GPIO, SX_DIO1_PIN, pudNone, SX_DIO1IrqHandler};
    void WriteReg(uint8_t RegAddr, uint8_t Value);
    uint8_t ReadReg (uint8_t RegAddr);
    void WriteFifo(uint8_t *ptr, uint8_t Sz);
    void ReadFifo(uint8_t *ptr, uint8_t Sz);
    // Middle level
    void Reset();
    void RxChainCalibration();
    void SetOpMode(uint8_t opMode);
    void SetTxPower(int8_t power);
    // Modes
    void EnterTXLora();
    void EnterStandby() { SetOpMode(RF_OPMODE_STANDBY); }
    void EnterSleep()   { SetOpMode(RF_OPMODE_SLEEP); }
public:
    uint32_t Channel;
    bool IqInverted = false, FreqHopOn = false;
    uint8_t FreqHopPeriod = 0;
    void SetChannel(uint32_t freq);
    void SetupTxConfigLora(int8_t power, SXLoraBW_t bandwidth, SXSpreadingFactor_t SpreadingFactor, SXCodingRate_t coderate, bool ImplicitHeaderModeOn, uint16_t preambleLen);
    uint8_t Init();
    void TransmitByLora(uint8_t *ptr, uint8_t Sz);
    void PrintState();
};

extern Lora_t Lora;
