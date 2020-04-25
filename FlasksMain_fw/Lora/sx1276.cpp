/*
 * sx1276.cpp
 *
 *  Created on: 18 апр. 2020 г.
 *      Author: layst
 */

#include "sx1276.h"
#include "shell.h"
#include "sx1276Regs-Fsk.h"
#include "sx1276Regs-LoRa.h"
#include "sx1276_Settings.h"

#define XTAL_FREQ   32000000
#define FREQ_STEP   61.03515625

static Spi_t ISpi{LORA_SPI};

uint8_t SX1276_t::Init() {
    PinSetupAnalog   (LORA_NRESET); // Must be floating during POR sequence
    PinSetupOut      (LORA_NSS, omPushPull);
    PinSetupAlterFunc(LORA_SCK);
    PinSetupAlterFunc(LORA_MISO);
    PinSetupAlterFunc(LORA_MOSI);
//    IGdo0.Init(ttFalling);
    NssHi();
    chThdSleepMilliseconds(10); // Let it wake after power-on
    // Reset it
    PinSetupOut(LORA_NRESET, omPushPull);
    PinSetLo(LORA_NRESET);
    chThdSleepMilliseconds(1);
    PinSetupAnalog(LORA_NRESET);
    chThdSleepMilliseconds(6); // Let it wake

    // ==== SPI ====
    // MSB first, master, ClkLowIdle, FirstEdge, Baudrate no more than 10MHz
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 10000000);
    ISpi.Enable();

//    Printf("Lora %X\r", ReadReg(0x27));

    RxChainCalibration();
    SetOpMode(RF_OPMODE_SLEEP);

    // Init IRQs
    // TODO

    for(int i=0; i < (sizeof(RadioRegsInit) / sizeof(RadioRegisters_t)); i++ ) {
        SetModem(RadioRegsInit[i].Modem);
        WriteReg(RadioRegsInit[i].Addr, RadioRegsInit[i].Value);
    }

    SetModem(MODEM_FSK);
    Settings.State = RF_IDLE;

    return retvOk;
}

#if 1 // ============================= Rx / Tx =================================
void SX1276_t::TransmitCarrier(uint32_t freq, int8_t power, sysinterval_t Duration) {
    SetFreq(freq);
    // FSK, power, Dev=0, BW=0, datarate=4800, coderate=0, preamble=5. no fixlen. no crc
    SetTxConfig(MODEM_FSK, power, 0, 0, 4800, 0, 5, false, false, 0, 0, 0, Duration);

    ModifyReg(REG_PACKETCONFIG2, RF_PACKETCONFIG2_DATAMODE_MASK, 0);

    // Disable radio interrupts TODO
//    WriteReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_11 | RF_DIOMAPPING1_DIO1_11);
//    WriteReg(REG_DIOMAPPING2, RF_DIOMAPPING2_DIO4_10 | RF_DIOMAPPING2_DIO5_10);

    Settings.State = RF_TX_RUNNING;
    StartTmrTimeout(Duration);
    SetOpMode(RF_OPMODE_TRANSMITTER);
}
#endif

#if 1 // ============================ Timers ===================================
void TmrTimeoutCallback(void *p) { ((SX1276_t*)p)->OnTimeout(); }

void SX1276_t::OnTimeout() {

}

void SX1276_t::StartTmrTimeout(sysinterval_t Timeout_st) {
    chVTSet(&TmrTimeout, Timeout_st, TmrTimeoutCallback, this);
}

#endif

#if 1 // =========================== Settings ==================================
/*
 * Performs the Rx chain calibration for HF bands
 * Must be called just after the reset so all registers are at their default values
 */
void SX1276_t::RxChainCalibration() {
    // Save context
    uint8_t regPaConfigInitVal = ReadReg(REG_PACONFIG);
    uint32_t vmsb = ReadReg(REG_FRFMSB);
    uint32_t vmid = ReadReg(REG_FRFMID);
    uint32_t vlsb = ReadReg(REG_FRFLSB);
    uint32_t initialFreq = (double)((vmsb<<16) | (vmid<<8) | vlsb) * (double)FREQ_STEP;
    // Cut the PA just in case, RFO output, power = -1 dBm
    WriteReg(REG_PACONFIG, 0x00);
    // Set Frequency in HF band
    SetFreq(868000000);
    // Launch Rx chain calibration for HF band
    ModifyReg(REG_IMAGECAL, RF_IMAGECAL_IMAGECAL_MASK, RF_IMAGECAL_IMAGECAL_START);
    while((ReadReg(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_RUNNING) == RF_IMAGECAL_IMAGECAL_RUNNING);
    Printf("Calibration done\r");
    // Restore context
    WriteReg(REG_PACONFIG, regPaConfigInitVal);
    SetFreq(initialFreq);
}

void SX1276_t::SetFreq(uint32_t Freq) {
    Freq = (uint32_t)((double)Freq / (double)FREQ_STEP);
    WriteReg(REG_FRFMSB, (uint8_t)((Freq >> 16) & 0xFF));
    WriteReg(REG_FRFMID, (uint8_t)((Freq >> 8) & 0xFF));
    WriteReg(REG_FRFLSB, (uint8_t)(Freq & 0xFF));
}

void SX1276_t::SetOpMode(uint8_t opMode) {
    ModifyReg(REG_OPMODE, RF_OPMODE_MASK, opMode);
}

void SX1276_t::SetModem(RadioModems_t modem) {
    if((ReadReg(REG_OPMODE) & RFLR_OPMODE_LONGRANGEMODE_ON) != 0) Settings.Modem = MODEM_LORA;
    else Settings.Modem = MODEM_FSK;

    if(Settings.Modem == modem) return;

    Settings.Modem = modem;
    SetOpMode(RF_OPMODE_SLEEP);

    switch(Settings.Modem) {
        default:
        case MODEM_FSK:
            ModifyReg(REG_OPMODE, RFLR_OPMODE_LONGRANGEMODE_MASK, RFLR_OPMODE_LONGRANGEMODE_OFF);
            WriteReg(REG_DIOMAPPING1, 0x00);
            WriteReg(REG_DIOMAPPING2, 0x30); // DIO5=ModeReady  TODO
            break;
        case MODEM_LORA:
            ModifyReg(REG_OPMODE, RFLR_OPMODE_LONGRANGEMODE_MASK, RFLR_OPMODE_LONGRANGEMODE_ON);
            WriteReg( REG_DIOMAPPING1, 0x00);
            WriteReg( REG_DIOMAPPING2, 0x00);
            break;
    }
}

void SX1276_t::SetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                        uint32_t bandwidth, uint32_t datarate,
                        uint8_t coderate, uint16_t preambleLen,
                        bool fixLen, bool crcOn, bool freqHopOn,
                        uint8_t hopPeriod, bool iqInverted, sysinterval_t timeout_st) {
    SetModem(modem);
    SetTxPower(power);


}

void SX1276_t::SetTxPower(int8_t power) {
    uint8_t paConfig = 0;
    uint8_t paDac = 0;

    paConfig = ReadReg(REG_PACONFIG);
    paDac = ReadReg(REG_PADAC);

    paConfig = (paConfig & RF_PACONFIG_PASELECT_MASK) | GetPaSelect(Settings.Channel);

    if((paConfig & RF_PACONFIG_PASELECT_PABOOST) == RF_PACONFIG_PASELECT_PABOOST) {
        if(power > 17) paDac = (paDac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_ON;
        else           paDac = (paDac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_OFF;
        if((paDac & RF_PADAC_20DBM_ON) == RF_PADAC_20DBM_ON) {
            if(power < 5)  power = 5;
            if(power > 20) power = 20;
            paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t) ((uint16_t) (power - 5) & 0x0F);
        }
        else {
            if(power < 2)  power = 2;
            if(power > 17) power = 17;
            paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t) ((uint16_t) (power - 2) & 0x0F);
        }
    }
    else {
        if(power > 0) {
            if(power > 15) power = 15;
            paConfig = (paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK) | (7 << 4) | (power);
        }
        else {
            if(power < -4) power = -4;
            paConfig = (paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK) | (0 << 4) | (power + 4);
        }
    }
    WriteReg(REG_PACONFIG, paConfig);
    WriteReg(REG_PADAC, paDac);
}


#endif

#if 1 // ============================= Low level ===============================
uint8_t SX1276_t::ReadReg(uint8_t Addr) {
    NssLo();
    ISpi.ReadWriteByte(Addr);
    uint8_t Reply = ISpi.ReadWriteByte(0);
    NssHi();
    return Reply;
}

void SX1276_t::WriteReg(uint8_t Addr, uint8_t Value) {
    NssLo();
    ISpi.ReadWriteByte(Addr | 0x80);
    ISpi.ReadWriteByte(Value);
    NssHi();
}

void SX1276_t::ModifyReg(uint8_t Addr, uint8_t MaskAnd, uint8_t MaskOr) {
    WriteReg(Addr, (ReadReg(Addr) & MaskAnd) | MaskOr);
}
#endif
