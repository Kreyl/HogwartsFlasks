/*
 * radio_lvl1.cpp
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#include "radio_lvl1.h"
#include "Lora.h"
#include "uart.h"
#include "kl_time.h"
#include "led.h"
#include "Sequences.h"
#include "Points.h"


//#define DBG_PINS

#ifdef DBG_PINS
#define DBG_GPIO1   GPIOB
#define DBG_PIN1    10
#define DBG1_SET()  PinSetHi(DBG_GPIO1, DBG_PIN1)
#define DBG1_CLR()  PinSetLo(DBG_GPIO1, DBG_PIN1)
#define DBG_GPIO2   GPIOB
#define DBG_PIN2    9
#define DBG2_SET()  PinSetHi(DBG_GPIO2, DBG_PIN2)
#define DBG2_CLR()  PinSetLo(DBG_GPIO2, DBG_PIN2)
#else
#define DBG1_SET()
#define DBG1_CLR()
#endif

rLevel1_t Radio;
uint8_t CheckAndSetTime(DateTime_t &dt);

#if 1 // ================================ Task =================================
static THD_WORKING_AREA(warLvl1Thread, 256);
__noreturn
static void rLvl1Thread(void *arg) {
    chRegSetThreadName("rLvl1");
    Radio.ITask();
}

__noreturn
void rLevel1_t::ITask() {
    while(true) {
        uint8_t len = RPKT_LEN;
        Lora.SetupRxConfigLora(LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit, len);
        uint8_t r = Lora.ReceiveByLora((uint8_t*)&pkt, &len, 27000);
        if(r == retvOk) {
            Printf("SNR: %d; RSSI: %d; Len: %u\r", Lora.RxParams.SNR, Lora.RxParams.RSSI, len);
            Printf("%u; ", pkt.cmd);
            if(pkt.cmd == kcmd_set_shown or pkt.cmd == kcmd_set_hidden) {
                Printf("%d %d %d %d; hidden %u\r", pkt.grif, pkt.slyze, pkt.rave, pkt.huff, pkt.cmd);
                if(pkt.grif  >= -999 and pkt.grif  <= 9999 and
                   pkt.slyze >= -999 and pkt.slyze <= 9999 and
                   pkt.rave  >= -999 and pkt.rave  <= 9999 and
                   pkt.huff  >= -999 and pkt.huff  <= 9999
                ) {
                    Points::Set(Points::Values(pkt.grif, pkt.slyze, pkt.rave, pkt.huff, pkt.cmd));
                    // Send reply
                    pkt.reply = 0xCa110fEa;
                    pkt.salt_reply = RPKT_SALT;
                    Lora.SetupTxConfigLora(TX_PWR_dBm, LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit);
                    Lora.TransmitByLora((uint8_t*)&pkt, RPKT_LEN);
                }
                else Printf("Bad Values\r");
            }
            else if(pkt.cmd == kcmd_set_time) {
                DateTime_t dt;
                dt.Year = pkt.year;
                dt.Month = pkt.month;
                dt.Day = pkt.day;
                dt.H = pkt.hours;
                dt.M = pkt.minutes;
                dt.S = 0;
                pkt.reply = CheckAndSetTime(dt);
                pkt.salt_reply = RPKT_SALT;
                Lora.SetupTxConfigLora(TX_PWR_dBm, LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit);
                Lora.TransmitByLora((uint8_t*)&pkt, RPKT_LEN);
            }
            else Printf("unknown\r");
        }
        else if(r == retvCRCError) Printf("CRC Err\r");
    } // while true
}
#endif // task

#if 1 // ============================
uint8_t rLevel1_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
    PinSetupOut(DBG_GPIO2, DBG_PIN2, omPushPull);
#endif
    if(Lora.Init() == retvOk) {
        Lora.SetChannel(868000000);
        Lora.SetupTxConfigLora(TX_PWR_dBm, LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit);
        Lora.SetupRxConfigLora(LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit, 64);
        // Thread
        chThdCreateStatic(warLvl1Thread, sizeof(warLvl1Thread), HIGHPRIO, (tfunc_t)rLvl1Thread, NULL);
        return retvOk;
    }
    else return retvFail;
}

const char* strBW[3] = {"125kHz", "250kHz", "500kHz"};
const SXLoraBW_t bw[3] = {bwLora125kHz, bwLora250kHz, bwLora500kHz};
const char* strSF[7] = {"64cps", "128cps", "256cps", "512cps", "1024cps", "2048cps", "4096cps"};
const SXSpreadingFactor_t sf[7] = {
        sprfact64chipsPersym, sprfact128chipsPersym, sprfact256chipsPersym,
        sprfact512chipsPersym, sprfact1024chipsPersym, sprfact2048chipsPersym,
        sprfact4096chipsPersym};

const char* strCR[4] = {"4s5", "4s6", "4s7", "4s8"};
const SXCodingRate_t CR[4] = {coderate4s5, coderate4s6, coderate4s7, coderate4s8};

void rLevel1_t::SetupTxConfigLora(uint8_t power, uint8_t BandwidthIndx,
        uint8_t SpreadingFactorIndx, uint8_t CoderateIndx) {
    Lora.SetupTxConfigLora(power, bw[BandwidthIndx], sf[SpreadingFactorIndx], CR[CoderateIndx], hdrmodeExplicit);
    Printf("Pwr=%d; %S; %S; %S\r\n", power, strBW[BandwidthIndx], strSF[SpreadingFactorIndx], strCR[CoderateIndx]);
}
#endif
