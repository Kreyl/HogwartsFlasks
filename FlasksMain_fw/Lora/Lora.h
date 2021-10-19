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

#define SX_RF_OUT_PABOOST   TRUE  // Set to TRUE if RF out is PA_BOOST, and to FALSE if RFO_HF
#define LORA_FIFO_SZ        64 // const, see datasheet

// When using LoRa modem only bandwidths 125, 250 and 500 kHz are supported
// Each doubling of the bandwidth correlates to almost 3dB less link budget.
enum SXLoraBW_t : uint8_t {
    bwLora125kHz = 0b0111, // their ID: 7
    bwLora250kHz = 0b1000, // their ID: 8
    bwLora500kHz = 0b1001  // their ID: 9
};

enum SXCodingRate_t : uint8_t {
    coderate4s5 = 1,
    coderate4s6 = 2,
    coderate4s7 = 3,
    coderate4s8 = 4
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

enum SXHeaderMode_t : uint8_t {
    hdrmodeExplicit = 0,
    hdrmodeImplicit = 1
};

void SX_DIO0IrqHandler();

class Lora_t {
private:
    const PinIrq_t DIO0{SX_DIO0_GPIO, SX_DIO0_PIN, pudNone, SX_DIO0IrqHandler};
    void WriteReg(uint8_t RegAddr, uint8_t Value);
    uint8_t ReadReg (uint8_t RegAddr);
    void WriteFifo(uint8_t *ptr, uint8_t Sz);
    void ReadFifo(uint8_t *ptr, uint8_t Sz);
    // Inner use
//    virtual_timer_t ITmr;
    thread_reference_t ThdRef = nullptr;

    // Middle level
    void RxChainCalibration();
    void SetOpMode(uint8_t opMode);
    void SetTxPower(uint8_t power);
    void SetLoraModem();
public:
    struct {
        uint32_t Channel;
        SXLoraBW_t Bandwidth;
        bool IqInverted = false;
        bool FskFreqHopOn = false;
        uint8_t FreqHopPeriod = 0;
        ftVoidVoid RxCallback;
    } Settings;

    struct {
        int16_t SNR, RSSI;
    } RxParams;

    void SetChannel(uint32_t freq);
    bool IsChannelFreeLora(uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime);
    void SetupTxConfigLora(uint8_t power, SXLoraBW_t bandwidth,
            SXSpreadingFactor_t SpreadingFactor, SXCodingRate_t coderate,
            SXHeaderMode_t HeaderMode);
    void SetupRxConfigLora(SXLoraBW_t bandwidth,
            SXSpreadingFactor_t SpreadingFactor, SXCodingRate_t coderate,
            SXHeaderMode_t HeaderMode, uint8_t payloadLen);

    uint8_t Init();
    uint32_t TransmitByLora(uint8_t *ptr, uint8_t Sz);
    uint8_t ReceiveByLora(uint8_t *ptr, uint8_t *pSz, uint32_t Timeout_ms);

    // Modes
    void EnterStandby() { SetOpMode(RF_OPMODE_STANDBY); }
    void EnterSleep()   { SetOpMode(RF_OPMODE_SLEEP); }

    // Dbg
    void PrintState();
    void PrintRegs();
    // Inner use
    void IIrqHandler();
};

extern Lora_t Lora;
