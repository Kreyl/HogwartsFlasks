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

enum RadioModems_t { MODEM_FSK = 0, MODEM_LORA };

class SX1276_t {
private:
    // Low level
    void NssHi() { PinSetHi(LORA_NSS); }
    void NssLo() { PinSetLo(LORA_NSS); }
    uint8_t ReadReg(uint8_t Addr);
    void WriteReg(uint8_t Addr, uint8_t Value);
    void ModifyReg(uint8_t Addr, uint8_t MaskAnd, uint8_t MaskOr);
    // Mid level
    virtual_timer_t TmrTimeout;
    void StartTmrTimeout(sysinterval_t Timeout_st);
    void RxChainCalibration();
    void SetFreq(uint32_t Freq);
    void SetOpMode(uint8_t opMode);
    void SetModem(RadioModems_t modem);
    void SetTxPower(int8_t power);
    void SetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                            uint32_t bandwidth, uint32_t datarate,
                            uint8_t coderate, uint16_t preambleLen,
                            bool fixLen, bool crcOn, bool freqHopOn,
                            uint8_t hopPeriod, bool iqInverted, sysinterval_t timeout_st);
public:
    uint8_t Init();
    void TransmitCarrier(uint32_t freq, int8_t power, sysinterval_t Duration);


    enum RadioState_t {
        RF_IDLE = 0,   // The radio is idle
        RF_RX_RUNNING, // The radio is in reception state
        RF_TX_RUNNING, // The radio is in transmission state
        RF_CAD,        // The radio is doing channel activity detection
    };

    struct {
        RadioState_t  State;
        RadioModems_t Modem;
    } Settings;

    // inner use
    void OnTimeout();
};
