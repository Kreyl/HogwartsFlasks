/*
 * sx1276_Settings.h
 *
 *  Created on: 19 апр. 2020 г.
 *      Author: layst
 */

#pragma once

struct RadioRegisters_t {
    RadioModems_t Modem;
    uint8_t       Addr;
    uint8_t       Value;
};

const RadioRegisters_t RadioRegsInit[] = {
    { MODEM_FSK , REG_LNA                , 0x23 },
    { MODEM_FSK , REG_RXCONFIG           , 0x1E },
    { MODEM_FSK , REG_RSSICONFIG         , 0xD2 },
    { MODEM_FSK , REG_AFCFEI             , 0x01 },
    { MODEM_FSK , REG_PREAMBLEDETECT     , 0xAA },
    { MODEM_FSK , REG_OSC                , 0x07 },
    { MODEM_FSK , REG_SYNCCONFIG         , 0x12 },
    { MODEM_FSK , REG_SYNCVALUE1         , 0xC1 },
    { MODEM_FSK , REG_SYNCVALUE2         , 0x94 },
    { MODEM_FSK , REG_SYNCVALUE3         , 0xC1 },
    { MODEM_FSK , REG_PACKETCONFIG1      , 0xD8 },
    { MODEM_FSK , REG_FIFOTHRESH         , 0x8F },
    { MODEM_FSK , REG_IMAGECAL           , 0x02 },
    { MODEM_FSK , REG_DIOMAPPING1        , 0x00 },
    { MODEM_FSK , REG_DIOMAPPING2        , 0x30 },
    { MODEM_LORA, REG_LR_PAYLOADMAXLENGTH, 0x40 },
};
