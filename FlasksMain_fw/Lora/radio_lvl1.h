#pragma once

#include "kl_lib.h"
#include "ch.h"
#include "kl_buf.h"
#include "uart.h"
#include "MsgQ.h"
#include "Lora.h"

#if 1 // =========================== Pkt_t =====================================
union rPkt_t {
    struct {
        int16_t Grif, Slyze, Rave, Huff;
        uint32_t SaltPnt;
    } __attribute__((__packed__));
    struct {
        uint32_t Reply;
        uint32_t SaltRply;
    } __attribute__((__packed__));
//    rPkt_t& operator = (const rPkt_t &Right) {
//        DW32 = Right.DW32;
//        return *this;
//    }
} __attribute__ ((__packed__));
#endif

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

#if 0 // ============================= RX Table ================================
#define RXTABLE_SZ              50
#define RXT_PKT_REQUIRED        TRUE
class RxTable_t {
private:
#if RXT_PKT_REQUIRED
    rPkt_t IBuf[RXTABLE_SZ];
#else
    uint8_t IdBuf[RXTABLE_SZ];
#endif
public:
    uint32_t Cnt = 0;
#if RXT_PKT_REQUIRED
    void AddOrReplaceExistingPkt(rPkt_t &APkt) {
        chSysLock();
        for(uint32_t i=0; i<Cnt; i++) {
            if((IBuf[i].ID == APkt.ID) and (IBuf[i].RCmd == APkt.RCmd)) {
                if(IBuf[i].Rssi < APkt.Rssi) IBuf[i] = APkt; // Replace with newer pkt if RSSI is stronger
                chSysUnlock();
                return;
            }
        }
        // Same ID not found
        if(Cnt < RXTABLE_SZ) {
            IBuf[Cnt] = APkt;
            Cnt++;
        }
        chSysUnlock();
    }

    uint8_t GetPktByID(uint8_t ID, rPkt_t *ptr) {
        for(uint32_t i=0; i<Cnt; i++) {
            if(IBuf[i].ID == ID) {
                *ptr = IBuf[i];
                return retvOk;
            }
        }
        return retvFail;
    }

    bool IDPresents(uint8_t ID) {
        for(uint32_t i=0; i<Cnt; i++) {
            if(IBuf[i].ID == ID) return true;
        }
        return false;
    }

    rPkt_t& operator[](const int32_t Indx) {
        return IBuf[Indx];
    }
#else
    void AddId(uint8_t ID) {
        if(Cnt >= RXTABLE_SZ) return;   // Buffer is full, nothing to do here
        for(uint32_t i=0; i<Cnt; i++) {
            if(IdBuf[i] == ID) return;
        }
        IdBuf[Cnt] = ID;
        Cnt++;
    }

#endif

    void Print() {
        Printf("RxTable Cnt: %u\r", Cnt);
        for(uint32_t i=0; i<Cnt; i++) {
#if RXT_PKT_REQUIRED
//            Printf("ID: %u; State: %u\r", IBuf[i].ID, IBuf[i].State);
#else
            Printf("ID: %u\r", IdBuf[i]);
#endif
        }
    }
};
#endif

#if 0 // ========================== Message queue ==============================
#define R_MSGQ_LEN      9
enum RmsgId_t { rmsgEachOthRx, rmsgEachOthTx, rmsgEachOthSleep, rmsgPktRx, rmsgFar };
struct RMsg_t {
    RmsgId_t Cmd;
    uint8_t Value;
    RMsg_t() : Cmd(rmsgEachOthSleep), Value(0) {}
    RMsg_t(RmsgId_t ACmd) : Cmd(ACmd), Value(0) {}
    RMsg_t(RmsgId_t ACmd, uint8_t AValue) : Cmd(ACmd), Value(AValue) {}
} __attribute__((packed));
#endif

class rLevel1_t {
private:
//    RxTable_t RxTable1, RxTable2, *RxTableW = &RxTable1;
public:
    uint8_t TxBuf[LORA_FIFO_SZ];
    rPkt_t PktRx, PktTx;
//    EvtMsgQ_t<RMsg_t, R_MSGQ_LEN> RMsgQ;
#if 0
    RxTable_t& GetRxTable() {
        chSysLock();
        RxTable_t* RxTableR;
        // Switch tables
        if(RxTableW == &RxTable1) {
            RxTableW = &RxTable2;
            RxTableR = &RxTable1;
        }
        else {
            RxTableW = &RxTable1;
            RxTableR = &RxTable2;
        }
        RxTableW->Cnt = 0; // Clear it
        chSysUnlock();
        return *RxTableR;
    }
#endif
    uint8_t Init();
    void SetupTxConfigLora(uint8_t power, uint8_t BandwidthIndx, uint8_t SpreadingFactorIndx, uint8_t CoderateIndx);
    void TransmitBuf(uint8_t Sz) { Lora.TransmitByLora(TxBuf, Sz); }
    // Inner use
    void ITask();
};

extern rLevel1_t Radio;
