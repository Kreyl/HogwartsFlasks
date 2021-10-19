/*
 * radio_lvl1.cpp
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#include "radio_lvl1.h"
#include "Lora.h"
#include "uart.h"

#include "led.h"
#include "Sequences.h"
#include "Points.h"


#define DBG_PINS

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
        uint8_t Len = RPKT_LEN;
        Lora.SetupRxConfigLora(LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit, Len);
        uint8_t Rslt = Lora.ReceiveByLora((uint8_t*)&PktRx, &Len, 27000);
        if(Rslt == retvOk and PktRx.SaltPnt == RPKT_SALT) { // Additional check
            Printf("SNR: %d; RSSI: %d; Len: %u\r", Lora.RxParams.SNR, Lora.RxParams.RSSI, Len);
            Printf("%A\r", &PktRx, RPKT_LEN, ' ');
            if(PktRx.Grif  >= -999 and PktRx.Grif  <= 9999 and
               PktRx.Slyze >= -999 and PktRx.Slyze <= 9999 and
               PktRx.Rave  >= -999 and PktRx.Rave  <= 9999 and
               PktRx.Huff  >= -999 and PktRx.Huff  <= 9999
            ) {
                Points::Set(PktRx.Grif, PktRx.Slyze, PktRx.Rave, PktRx.Huff);
                // Send reply
                PktTx.Reply = 0xCA115EA1;
                PktTx.SaltRply = RPKT_SALT;
                //chThdSleepMilliseconds(1);
                Lora.SetupTxConfigLora(TX_PWR_dBm, LORA_BW, LORA_SPREADRFCT, LORA_CODERATE, hdrmodeExplicit);
                Lora.TransmitByLora((uint8_t*)&PktTx, RPKT_LEN);
            }
            else Printf("Bad Values: %d %d %d %d\r", PktRx.Grif, PktRx.Slyze, PktRx.Rave, PktRx.Huff);
        }
        else if(Rslt == retvCRCError) Printf("CRC Err\r");
    } // while true
}
#endif // task

#if 1 // ============================
uint8_t rLevel1_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
    PinSetupOut(DBG_GPIO2, DBG_PIN2, omPushPull);
#endif
    //RMsgQ.Init();
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
