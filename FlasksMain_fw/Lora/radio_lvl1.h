#ifndef RADIO_LVL1_H__
#define RADIO_LVL1_H__

#include "kl_lib.h"
#include "ch.h"
#include "kl_buf.h"
#include "uart.h"
#include "MsgQ.h"
#include "Lora.h"

#if 1 // =========================== Pkt_t =====================================
#pragma pack(push, 1)
union rPkt_t {
    struct {
        uint32_t reply;
        uint32_t salt_reply;
    };
    struct {
        uint8_t cmd;
        union {
            struct { int16_t grif, slyze, rave, huff; };
            struct {
                uint16_t year;
                uint8_t month, day, hours, minutes;
            };
        };
    };
};
#pragma pack(pop)
#endif

static const uint8_t kcmd_set_shown = 1;
static const uint8_t kcmd_set_hidden = 0;
static const uint8_t kcmd_set_time = 7;

#define RPKT_SALT   0xF1170511 // Fly to sly
#define RPKT_LEN    sizeof(rPkt_t)

#if 1 // =================== Channels, cycles, Rssi  ===========================
#define RCHNL_HZ        868000000

// [2; 20]
#define TX_PWR_dBm      11
// bwLora125kHz, bwLora250kHz, bwLora500kHz
#define LORA_BW         bwLora250kHz
// sprfact64chipsPersym, sprfact128chipsPersym, sprfact256chipsPersym, sprfact512chipsPersym,
// sprfact1024chipsPersym, sprfact2048chipsPersym, sprfact4096chipsPersym
#define LORA_SPREADRFCT sprfact1024chipsPersym
// coderate4s5, coderate4s6, coderate4s7, coderate4s8
#define LORA_CODERATE   coderate4s8

#endif

class rLevel1_t {
public:
    uint8_t tx_buf[LORA_FIFO_SZ];
    rPkt_t pkt;
    uint8_t Init();
    void SetupTxConfigLora(uint8_t power, uint8_t BandwidthIndx, uint8_t SpreadingFactorIndx, uint8_t CoderateIndx);
    void TransmitBuf(uint8_t Sz) { Lora.TransmitByLora(tx_buf, Sz); }
    // Inner use
    void ITask();
};

extern rLevel1_t Radio;

#endif //RADIO_LVL1_H__
