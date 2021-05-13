/*
 * Lora.cpp
 *
 *  Created on: 10 мая 2021 г.
 *      Author: layst
 */

#include "Lora.h"
#include "board.h"
#include "kl_lib.h"
#include "shell.h"
#include "sx1276Regs-Fsk.h"
#include "sx1276Regs-LoRa.h"
#include "sx1276_Settings.h"

Lora_t Lora;

// SX1276 definitions
#define XTAL_FREQ       32000000
#define FREQ_STEP       61.03515625
#define RX_BUFFER_SIZE  256

static const Spi_t ISpi {SX_SPI};

static inline void NssHi() { PinSetHi(SX_NSS); }
static inline void NssLo() { PinSetLo(SX_NSS); }


uint8_t Lora_t::Init() {
    // ==== GPIO ====
    // Reset: must be left floating for 10ms after power-on
    PinSetupAnalog(SX_NRESET);
    chThdSleepMilliseconds(11); // Wait more than 10ms
    PinSetupOut(SX_NRESET, omPushPull);
    PinSetLo(SX_NRESET);
    chThdSleepMilliseconds(1); // Wait more than 100us

    // SPI
    PinSetupOut      (SX_NSS, omPushPull);
    NssHi();
    PinSetupAlterFunc(SX_SCK);
    PinSetupAlterFunc(SX_MISO);
    PinSetupAlterFunc(SX_MOSI);
    // MSB first, master, ClkLowIdle, FirstEdge, Baudrate no more than 10MHz
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 1000000, bitn8);
    ISpi.Enable();

    // Release Reset
    PinSetHi(SX_NRESET);
    chThdSleepMilliseconds(7); // Wait more than 5ms

    // Check communication
    uint8_t ver = ReadReg(0x42);
    if(ver != 0x12) {
        Printf("Lora ver err: %X\r", ReadReg(0x42));
        return retvFail;
    }

    RxChainCalibration();
    SetOpMode(RF_OPMODE_SLEEP);

    return retvOk;
}

void Lora_t::RxChainCalibration() {
    uint8_t regPaConfigInitVal;
    uint32_t initialFreq;

    // Save context
    regPaConfigInitVal = ReadReg(REG_PACONFIG);
    initialFreq = (double) (((uint32_t) ReadReg( REG_FRFMSB) << 16)
            | ((uint32_t) ReadReg(REG_FRFMID) << 8)
            | ((uint32_t) ReadReg(REG_FRFLSB))) * (double) FREQ_STEP;

    // Cut the PA just in case, RFO output, power = -1 dBm
    WriteReg(REG_PACONFIG, 0x00);

    // Launch Rx chain calibration for LF band
//    WriteReg(REG_IMAGECAL, (ReadReg(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_MASK) | RF_IMAGECAL_IMAGECAL_START);
//    while((ReadReg(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_RUNNING) == RF_IMAGECAL_IMAGECAL_RUNNING);

    // Set a Frequency in HF band
    SetChannel(868000000);

    // Launch Rx chain calibration for HF band
    WriteReg(REG_IMAGECAL, (ReadReg(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_MASK) | RF_IMAGECAL_IMAGECAL_START);
    while(ReadReg(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_RUNNING);

    // Restore context
    WriteReg(REG_PACONFIG, regPaConfigInitVal);
    SetChannel(initialFreq);
}

void Lora_t::SetChannel(uint32_t freq) {
    Channel = freq;
    freq = (uint32_t)((double)freq / (double)FREQ_STEP);
    WriteReg(REG_FRFMSB, (uint8_t)((freq >> 16) & 0xFF));
    WriteReg(REG_FRFMID, (uint8_t)((freq >> 8)  & 0xFF ));
    WriteReg(REG_FRFLSB, (uint8_t)( freq        & 0xFF));
}

void Lora_t::SetOpMode(uint8_t opMode) {
    if(opMode == RF_OPMODE_SLEEP) SetAntSwLowPower(true);
    else {
        // Enable TCXO
        SetBoardTcxo(true);
        SetAntSwLowPower(false);
        SetAntSw(opMode);
    }
    WriteReg(REG_OPMODE, (ReadReg(REG_OPMODE) & RF_OPMODE_MASK) | opMode);
}


void Lora_t::WriteReg(uint8_t RegAddr, uint8_t Value) {
    RegAddr |= 0x80; // MSB==1 means write
    NssLo();
    ISpi.ReadWriteByte(RegAddr);
    ISpi.ReadWriteByte(Value);
    NssHi();
}

uint8_t Lora_t::ReadReg (uint8_t RegAddr) {
    uint8_t Value;
    NssLo();
    ISpi.ReadWriteByte(RegAddr);
    Value = ISpi.ReadWriteByte(0);
    NssHi();
    return Value;
}

