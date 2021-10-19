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

Lora_t Lora;

// SX1276 definitions
#define XTAL_FREQ       32000000
#define FREQ_STEP       61.03515625
#define RX_BUFFER_SIZE  256
#define RSSI_OFFSET_HF  -157 // Constant, see datasheet 5.5.5
#define PREAMBLE_LEN    8 // Everyone use it

static const Spi_t ISpi {SX_SPI};

static inline void NssHi() { PinSetHi(SX_NSS); }
static inline void NssLo() { PinSetLo(SX_NSS); }

// Precomputed FSK bandwidth registers values
struct FskBandwidth_t {
    uint32_t bandwidth;
    uint8_t  RegValue;
};

const FskBandwidth_t FskBandwidths[] = {
    { 2600  , 0x17 },
    { 3100  , 0x0F },
    { 3900  , 0x07 },
    { 5200  , 0x16 },
    { 6300  , 0x0E },
    { 7800  , 0x06 },
    { 10400 , 0x15 },
    { 12500 , 0x0D },
    { 15600 , 0x05 },
    { 20800 , 0x14 },
    { 25000 , 0x0C },
    { 31300 , 0x04 },
    { 41700 , 0x13 },
    { 50000 , 0x0B },
    { 62500 , 0x03 },
    { 83333 , 0x12 },
    { 100000, 0x0A },
    { 125000, 0x02 },
    { 166700, 0x11 },
    { 200000, 0x09 },
    { 250000, 0x01 },
    { 300000, 0x00 }, // Invalid Bandwidth
};

void SX_DIO0IrqHandler() { Lora.IIrqHandler(); }

void Lora_t::IIrqHandler() {
//    PrintfI("IrqDIO0\r");
    if(Settings.RxCallback != nullptr) {
        Settings.RxCallback();
        Settings.RxCallback = nullptr;
    }
    else chThdResumeI(&ThdRef, MSG_OK);  // NotNull check performed inside chThdResumeI
}

uint8_t Lora_t::Init() {
    // ==== GPIO ====
    // Reset: must be left floating for 10ms after power-on
    PinSetupAnalog(SX_NRESET);
    chThdSleepMilliseconds(11); // Wait more than 10ms
    PinSetupOut(SX_NRESET, omPushPull);
    PinSetLo(SX_NRESET);
    chThdSleepMilliseconds(1); // Wait more than 100us
    // Init IRQ
    DIO0.Init(ttRising);

    // SPI
    PinSetupOut      (SX_NSS, omPushPull);
    NssHi();
    PinSetupAlterFunc(SX_SCK);
    PinSetupAlterFunc(SX_MISO);
    PinSetupAlterFunc(SX_MOSI);
    // MSB first, master, ClkLowIdle, FirstEdge, Baudrate no more than 10MHz
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 10000000, bitn8);
    ISpi.Enable();

    // Release Reset
    PinSetHi(SX_NRESET);
    chThdSleepMilliseconds(6); // Wait more than 5ms

    // Check communication
    uint8_t ver = ReadReg(0x42);
    if(ver != 0x12) {
        Printf("Lora ver err: %X\r", ReadReg(0x42));
        return retvFail;
    }

    RxChainCalibration();
    EnterSleep();

    // ==== Write registers ====
    // Errata regs
    WriteReg(REG_LR_DETECTOPTIMIZE, 0b00000011);  // MSB should be set to 0 after each reset (POR on manual)
    // FSK regs
    WriteReg(REG_OPMODE,         0b00000000); // FSK mode, LoraRegs, HF mode, SLEEP
    WriteReg(REG_LNA,            0b00100011); // LNA Gain = max, def LNA curr, LNA boost on
    WriteReg(REG_RXCONFIG,       0b00011110); // No auto RX restart, AFC is performed at each receiver startup, LNA gain is controlled by the AGC, AGC when Preamble detect
    WriteReg(REG_RSSICONFIG,     0b11010010); // RssiOffset = 11010, RssiSmoothing = 8 samples
    WriteReg(REG_AFCFEI,         0b00000001); // AFC register is cleared at the beginning of the automatic AFC phase
    WriteReg(REG_PREAMBLEDETECT, 0b10101010); // Preamble detector on, prea sz = 2 bytes, Number or chip errors, 4 chips per bit = 0x0A
    WriteReg(REG_OSC,            0b00000111); // ClkOut = OFF
    WriteReg(REG_SYNCCONFIG,     0b00010010); // AutoRestartRxMode = Off, prea = 0xAA, SyncWord = on, SyncSz = 3bytes
    WriteReg(REG_SYNCVALUE1,     0xC1);       // 1st byte of Sync word
    WriteReg(REG_SYNCVALUE2,     0x94);       // 2nd byte of Sync word
    WriteReg(REG_SYNCVALUE3,     0xC1);       // 3d byte of Sync word
    WriteReg(REG_PACKETCONFIG1,  0b11011000); // Variable len, whitening on, Crc on, do not clear FIFO when CRC err, no addr filtering, standard CRC whitening
    WriteReg(REG_FIFOTHRESH,     0x8F);       // TxStartCondition = at least one byte in the FIFO, trigger FifoLevel interrupt, when number of bytes in FIFO >= 16
    WriteReg(REG_IMAGECAL,       0b00000010); // Calibration of the receiver depending on the temperature dis, TempThreshold = 10 dg, TempMonitor is on
    WriteReg(REG_DIOMAPPING1,    0x00);
    WriteReg(REG_DIOMAPPING2,    0x00);

    // LoRa regs
    WriteReg(REG_OPMODE, 0b10000000); // LoRa mode, LoraRegs, HF mode, SLEEP
    WriteReg(REG_LR_PAYLOADMAXLENGTH, 64);  // maximum payload length = FIFO sz
    WriteReg(REG_LR_IRQFLAGS, 0xFF); // Clear IRQ flags
//    PrintState();

    DIO0.EnableIrq(IRQ_PRIO_MEDIUM);
    Printf("Lora Init ok\r\n");
    return retvOk;
}

void Lora_t::PrintState() {
    Printf("PA: %02X; FifoPtr: %u; \r", ReadReg(0x09), ReadReg(0x0D));
    Printf("FifoCurr: %u; IrqFlags %02X\r", ReadReg(0x10), ReadReg(0x12));
    Printf("ModemStat: %02X; DIOMap1: %02X\r", ReadReg(0x18), ReadReg(0x40));
    Printf("Mode: %02X\r", ReadReg(0x01));
    Printf("DIO: %u %u\r", PinIsHi(SX_DIO0_GPIO, SX_DIO0_PIN), PinIsHi(SX_DIO1_GPIO, SX_DIO1_PIN));
}

void Lora_t::PrintRegs() {
    for(uint8_t i=1; i<=0x70; i++) PrintfI("0x%02X\r\n", ReadReg(i));
    PrintfI("########\r\n");
}

uint32_t Lora_t::TransmitByLora(uint8_t *ptr, uint8_t Sz) {
    if(Settings.IqInverted) {
        WriteReg(REG_LR_INVERTIQ, ((ReadReg(REG_LR_INVERTIQ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK) | RFLR_INVERTIQ_RX_OFF | RFLR_INVERTIQ_TX_ON));
        WriteReg(REG_LR_INVERTIQ2, RFLR_INVERTIQ2_ON);
    }
    else {
        WriteReg(REG_LR_INVERTIQ, ((ReadReg(REG_LR_INVERTIQ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK) | RFLR_INVERTIQ_RX_OFF | RFLR_INVERTIQ_TX_OFF));
        WriteReg(REG_LR_INVERTIQ2, RFLR_INVERTIQ2_OFF);
    }

    // Initializes the payload size
    WriteReg(REG_LR_PAYLOADLENGTH, Sz);

    // Full buffer used for Tx
    WriteReg(REG_LR_FIFOTXBASEADDR, 0);
    WriteReg(REG_LR_FIFOADDRPTR, 0);

    // FIFO operations can not take place in Sleep mode
    if((ReadReg(REG_OPMODE) & ~RF_OPMODE_MASK) == RF_OPMODE_SLEEP) {
        EnterStandby();
        chThdSleepMilliseconds(1);
    }
    // Write payload buffer
    WriteFifo(ptr, Sz);

    // ==== IRQs ==== 1 means disabled
    WriteReg(REG_LR_IRQFLAGSMASK, RFLR_IRQFLAGS_RXTIMEOUT |
                                  RFLR_IRQFLAGS_RXDONE |
                                  RFLR_IRQFLAGS_PAYLOADCRCERROR |
                                  RFLR_IRQFLAGS_VALIDHEADER |
//                                  RFLR_IRQFLAGS_TXDONE | // The only enabled irq
                                  RFLR_IRQFLAGS_CADDONE |
                                  RFLR_IRQFLAGS_FHSSCHANGEDCHANNEL |
                                  RFLR_IRQFLAGS_CADDETECTED);

    // DIO0 = TxDone irq
    WriteReg(REG_DIOMAPPING1, (ReadReg(REG_DIOMAPPING1) & RFLR_DIOMAPPING1_DIO0_MASK) | RFLR_DIOMAPPING1_DIO0_01);
    WriteReg(REG_LR_IRQFLAGS, 0xFF); // Clear all irq flags
    // ==== Enter TX and wait IRQ ====
    chSysLock();
    Settings.RxCallback = nullptr;
    systime_t Start = chVTGetSystemTimeX();
    SetOpMode(RF_OPMODE_TRANSMITTER);
    chThdSuspendS(&ThdRef); // Wait IRQ
    chSysUnlock();
    uint32_t t = TIME_I2MS(chVTTimeElapsedSinceX(Start));
    WriteReg(REG_LR_IRQFLAGS, 0xFF); // Clear all IRQs
    return t;
}

uint8_t Lora_t::ReceiveByLora(uint8_t *ptr, uint8_t *pSz, uint32_t Timeout_ms) {
    if(Settings.IqInverted) {
        WriteReg(REG_LR_INVERTIQ, ((ReadReg(REG_LR_INVERTIQ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK) | RFLR_INVERTIQ_RX_ON | RFLR_INVERTIQ_TX_OFF));
        WriteReg(REG_LR_INVERTIQ2, RFLR_INVERTIQ2_ON);
    }
    else {
        WriteReg(REG_LR_INVERTIQ, ((ReadReg(REG_LR_INVERTIQ ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK) | RFLR_INVERTIQ_RX_OFF | RFLR_INVERTIQ_TX_OFF));
        WriteReg(REG_LR_INVERTIQ2, RFLR_INVERTIQ2_OFF);
    }

    // ERRATA 2.3 - Receiver Spurious Reception of a LoRa Signal
    if(Settings.Bandwidth == bwLora500kHz) {
        WriteReg(REG_LR_DETECTOPTIMIZE, ReadReg(REG_LR_DETECTOPTIMIZE) | 0x80);
    }
    else { // 125/250 kHz
        WriteReg(REG_LR_DETECTOPTIMIZE, ReadReg(REG_LR_DETECTOPTIMIZE) & 0x7F);
        WriteReg(REG_LR_IFFREQ2, 0x00);
        WriteReg(REG_LR_IFFREQ1, 0x40);
    }

    // ==== IRQs ==== 1 means disabled
    WriteReg(REG_LR_IRQFLAGSMASK, RFLR_IRQFLAGS_RXTIMEOUT |
                                  //RFLR_IRQFLAGS_RXDONE |
                                  //RFLR_IRQFLAGS_PAYLOADCRCERROR |
                                  RFLR_IRQFLAGS_VALIDHEADER |
                                  RFLR_IRQFLAGS_TXDONE |
                                  RFLR_IRQFLAGS_CADDONE |
                                  RFLR_IRQFLAGS_FHSSCHANGEDCHANNEL |
                                  RFLR_IRQFLAGS_CADDETECTED);

    // DIO0=RxDone
    WriteReg(REG_DIOMAPPING1, (ReadReg(REG_DIOMAPPING1) & RFLR_DIOMAPPING1_DIO0_MASK) | RFLR_DIOMAPPING1_DIO0_00);
    WriteReg(REG_LR_IRQFLAGS, 0xFF); // Clear all irq flags

    // FIFO
    WriteReg(REG_LR_FIFORXBASEADDR, 0);
    WriteReg(REG_LR_FIFOADDRPTR, 0);
    WriteReg(REG_LR_PAYLOADLENGTH, *pSz);

    // ==== Enter RX and wait IRQ ====
    chSysLock();
    SetOpMode(RFLR_OPMODE_RECEIVER); // Receive forever
    msg_t Rslt = chThdSuspendTimeoutS(&ThdRef, TIME_MS2I(Timeout_ms)); // Wait IRQ
    chSysUnlock();
    // Will be here when IRQ will fire, or timeout occur - with appropriate message
    EnterStandby();         // Get out of RX mode
    if(Rslt == MSG_TIMEOUT) return retvTimeout; // Nothing received, timeout occured

    // ==== RxDone happened ====
    uint8_t irqFlags = ReadReg(REG_LR_IRQFLAGS);
    WriteReg(REG_LR_IRQFLAGS, 0xFF); // Clear all IRQs
    // Check CRC
    if(irqFlags & RFLR_IRQFLAGS_PAYLOADCRCERROR_MASK) return retvCRCError;

    // Read SNR and RSSI
    RxParams.SNR = (((int16_t)ReadReg(REG_LR_PKTSNRVALUE)) + 2) >> 2;
    int16_t rssiReg = ReadReg(REG_LR_PKTRSSIVALUE);
    if(RxParams.SNR < 0) RxParams.RSSI = RSSI_OFFSET_HF + rssiReg + (rssiReg >> 4) + RxParams.SNR;
    else                 RxParams.RSSI = RSSI_OFFSET_HF + rssiReg + (rssiReg >> 4);

    // Read FIFO
    uint8_t RegSz = ReadReg(REG_LR_RXNBBYTES);
    if(RegSz > *pSz) RegSz = *pSz;
    else *pSz = RegSz;
    ReadFifo(ptr, RegSz);

    return retvOk;
}

void Lora_t::RxChainCalibration() {
    uint8_t regPaConfigInitVal;
    uint32_t initialFreq;

    // Save context
    regPaConfigInitVal = ReadReg(REG_PACONFIG);
    initialFreq = (double) (((uint32_t) ReadReg(REG_FRFMSB) << 16)
                          | ((uint32_t) ReadReg(REG_FRFMID) << 8)
                          | ((uint32_t) ReadReg(REG_FRFLSB))) * (double)FREQ_STEP;

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

void Lora_t::SetLoraModem() {
    uint8_t opmode = ReadReg(REG_OPMODE);
    if((opmode & RFLR_OPMODE_LONGRANGEMODE_ON) == 0) { // FSK, switch to Lora
        WriteReg(REG_OPMODE, (opmode & RF_OPMODE_MASK) | RF_OPMODE_SLEEP); // Enter sleep
        WriteReg(REG_OPMODE,
                (opmode & RFLR_OPMODE_LONGRANGEMODE_MASK)
                        | RFLR_OPMODE_LONGRANGEMODE_ON);
        WriteReg(REG_DIOMAPPING1, 0x00);
        WriteReg(REG_DIOMAPPING2, 0x00);
    }
}

/*   Power: [2; 20]
 *   Bandwidth: bwLora125kHz, bwLora250kHz, bwLora500kHz
 *   SpreadingFactor: sprfact64chipsPersym, ..., sprfact4096chipsPersym
 *   coderate: coderate4s5, coderate4s6, coderate4s7, coderate4s8
 *   preambleLen
 */
void Lora_t::SetupTxConfigLora(uint8_t power, SXLoraBW_t bandwidth,
        SXSpreadingFactor_t SpreadingFactor, SXCodingRate_t coderate, SXHeaderMode_t HeaderMode) {
    if(SpreadingFactor == sprfact64chipsPersym and HeaderMode == hdrmodeExplicit) {
        Printf("Lora bad mode\r"); // See datasheet
        return;
    }

    SetLoraModem();
    SetTxPower(power);
    Settings.Bandwidth = bandwidth;

    bool LowDatarateOptimize =
            ((bandwidth == bwLora125kHz) and ((SpreadingFactor == sprfact2048chipsPersym) or (SpreadingFactor == sprfact4096chipsPersym))) ||
            ((bandwidth == bwLora250kHz) and ( SpreadingFactor == sprfact4096chipsPersym));

    WriteReg(REG_LR_MODEMCONFIG1, ((uint8_t)bandwidth << 4) | ((uint8_t)coderate << 1) | (uint8_t)HeaderMode);
    WriteReg(REG_LR_MODEMCONFIG2, ((uint8_t)SpreadingFactor << 4) | (1 << 2)); // RX CRC on
    WriteReg(REG_LR_MODEMCONFIG3, (LowDatarateOptimize? (1 << 3) : 0) | (1 << 2)); // Agc auto on

    WriteReg(REG_LR_PREAMBLEMSB, (PREAMBLE_LEN >> 8) & 0x00FF);
    WriteReg(REG_LR_PREAMBLELSB, PREAMBLE_LEN & 0xFF);

    if(SpreadingFactor == sprfact64chipsPersym) {
        WriteReg(REG_LR_DETECTOPTIMIZE, (ReadReg(REG_LR_DETECTOPTIMIZE) & RFLR_DETECTIONOPTIMIZE_MASK) | RFLR_DETECTIONOPTIMIZE_SF6);
        WriteReg(REG_LR_DETECTIONTHRESHOLD, RFLR_DETECTIONTHRESH_SF6);
    }
    else {
        WriteReg(REG_LR_DETECTOPTIMIZE, (ReadReg(REG_LR_DETECTOPTIMIZE) & RFLR_DETECTIONOPTIMIZE_MASK) | RFLR_DETECTIONOPTIMIZE_SF7_TO_SF12);
        WriteReg(REG_LR_DETECTIONTHRESHOLD, RFLR_DETECTIONTHRESH_SF7_TO_SF12);
    }
}

void Lora_t::SetupRxConfigLora(SXLoraBW_t bandwidth,
        SXSpreadingFactor_t SpreadingFactor, SXCodingRate_t coderate,
        SXHeaderMode_t HeaderMode, uint8_t payloadLen) {
    SetLoraModem();
    Settings.Bandwidth = bandwidth;

    bool LowDatarateOptimize =
            ((bandwidth == bwLora125kHz) and ((SpreadingFactor == sprfact2048chipsPersym) or (SpreadingFactor == sprfact4096chipsPersym))) ||
            ((bandwidth == bwLora250kHz) and ( SpreadingFactor == sprfact4096chipsPersym));

    WriteReg(REG_LR_MODEMCONFIG1, ((uint8_t)bandwidth << 4) | ((uint8_t)coderate << 1) | (uint8_t)HeaderMode);
    WriteReg(REG_LR_MODEMCONFIG2, ((uint8_t)SpreadingFactor << 4) | (1 << 2)); // RX CRC on
    WriteReg(REG_LR_MODEMCONFIG3, (LowDatarateOptimize? (1 << 3) : 0) | (1 << 2)); // Agc auto on

    WriteReg(REG_LR_SYMBTIMEOUTLSB, 0);

    WriteReg(REG_LR_PREAMBLEMSB, (PREAMBLE_LEN >> 8) & 0x00FF);
    WriteReg(REG_LR_PREAMBLELSB, PREAMBLE_LEN & 0xFF);

    if(HeaderMode == hdrmodeImplicit) WriteReg(REG_LR_PAYLOADLENGTH, payloadLen);

    // See ERRATA 2.1
    if(bandwidth == bwLora500kHz) {
        if(Settings.Channel > 862000000) {
            // ERRATA 2.1 - Sensitivity Optimization with a 500 kHz Bandwidth
            WriteReg(REG_LR_HIGHBWOPTIMIZE1, 0x02);
            WriteReg(REG_LR_HIGHBWOPTIMIZE2, 0x64);
        }
        else {
            // ERRATA 2.1 - Sensitivity Optimization with a 500 kHz Bandwidth
            WriteReg(REG_LR_HIGHBWOPTIMIZE1, 0x02);
            WriteReg(REG_LR_HIGHBWOPTIMIZE2, 0x7F);
        }
    }
    else {
        // ERRATA 2.1 - Sensitivity Optimization with a 500 kHz Bandwidth
        WriteReg(REG_LR_HIGHBWOPTIMIZE1, 0x03);
    }

    if(SpreadingFactor == sprfact64chipsPersym) {
        WriteReg(REG_LR_DETECTOPTIMIZE,
                          (ReadReg(REG_LR_DETECTOPTIMIZE) &
                            RFLR_DETECTIONOPTIMIZE_MASK) |
                            RFLR_DETECTIONOPTIMIZE_SF6);
        WriteReg(REG_LR_DETECTIONTHRESHOLD, RFLR_DETECTIONTHRESH_SF6);
    }
    else {
        WriteReg(REG_LR_DETECTOPTIMIZE,
                     (ReadReg( REG_LR_DETECTOPTIMIZE ) &
                     RFLR_DETECTIONOPTIMIZE_MASK ) |
                     RFLR_DETECTIONOPTIMIZE_SF7_TO_SF12);
        WriteReg(REG_LR_DETECTIONTHRESHOLD, RFLR_DETECTIONTHRESH_SF7_TO_SF12);
    }
}


void Lora_t::SetTxPower(uint8_t power) {
    uint8_t paConfig = 0;
    uint8_t paDac = 0;

    paConfig = ReadReg(REG_PACONFIG);
    paDac = ReadReg(REG_PADAC);

#if SX_RF_OUT_PABOOST
    paConfig = (paConfig & RF_PACONFIG_PASELECT_MASK) | RF_PACONFIG_PASELECT_PABOOST; // Rf out is connected to PA_BOOST
    if(power > 17) {
        paDac = (paDac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_ON;
        if(power < 5) power = 5;
        if(power > 20) power = 20;
        paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t) ((uint16_t) (power - 5) & 0x0F);
    }
    else {
        paDac = (paDac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_OFF;
        if(power < 2)  power = 2;
        if(power > 17) power = 17;
        paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t) (power - 2) & 0x0F);
    }
#else
    paConfig = (paConfig & RF_PACONFIG_PASELECT_MASK);
    if(power > 0) {
        if(power > 15) power = 15;
        paConfig = (paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK) | (7 << 4) | (power);
    }
    else {
        if(power < -4) power = -4;
        paConfig = (paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK) | (0 << 4) | (power + 4);
    }
#endif
    WriteReg(REG_PACONFIG, paConfig);
    WriteReg(REG_PADAC, paDac);
}

void Lora_t::SetChannel(uint32_t freq) {
    Settings.Channel = freq;
    freq = (uint32_t)((double)freq / (double)FREQ_STEP);
    WriteReg(REG_FRFMSB, (uint8_t)((freq >> 16) & 0xFF));
    WriteReg(REG_FRFMID, (uint8_t)((freq >> 8)  & 0xFF ));
    WriteReg(REG_FRFLSB, (uint8_t)( freq        & 0xFF));
}



void Lora_t::SetOpMode(uint8_t opMode) {
    WriteReg(REG_OPMODE, (ReadReg(REG_OPMODE) & RF_OPMODE_MASK) | opMode);
}

#if 1 // ====================== Read/Write Reg/FIFO ============================
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

void Lora_t::WriteFifo(uint8_t *ptr, uint8_t Sz) {
    NssLo();
    ISpi.ReadWriteByte(0x80); // Fifo addr = 0 | 0x80
    while(Sz--) ISpi.ReadWriteByte(*ptr++);
    NssHi();
}

void Lora_t::ReadFifo(uint8_t *ptr, uint8_t Sz) {
    NssLo();
    ISpi.ReadWriteByte(0); // Fifo addr = 0
    while(Sz--) *ptr++ = ISpi.ReadWriteByte(0);
    NssHi();
}
#endif
