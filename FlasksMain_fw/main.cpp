#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "led.h"
#include "ws2812b.h"
#include "kl_sd.h"
#include "kl_fs_utils.h"
#include "kl_time.h"
#include "kl_i2c.h"
#include "Mirilli.h"
//#include "usb_msdcdc.h"
#include "sdram.h"
#include "Lora.h"
#include "lcdtft.h"
#include "AviDecode.h"
#include "CS42L52.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

CS42L52_t Codec;

//static bool UsbPinWasHi = false;
LedBlinker_t Led{LED_PIN};
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic}; // Measure battery periodically

void SwitchTo27MHz();
void SwitchTo216MHz();
#endif

#if 1 // ================================ Code =================================
static enum AppState_t {stateIdle, statePlaying} AppState = stateIdle;
static const uint8_t Code[] = {1,2,3};

#define CODE_LEN            countof(Code)
#define BTN_CNT             8
#define ENTERED_MAX_LEN     9
#define CODE_TIMEOUT_S      4

class CodeChecker_t {
private:
    uint32_t Cnt = 0;
    uint8_t WhatEntered[ENTERED_MAX_LEN];
    bool IsCorrect() {
        if(Cnt != CODE_LEN) return false;
        for(uint32_t i=0; i<Cnt; i++) {
            if(WhatEntered[i] != Code[i]) return false;
        }
        return true;
    }
    int32_t Timeout = 0;
public:
    void OnBtnPress(uint8_t BtnId) {
        Printf("Btn %u\r", BtnId);
        WhatEntered[Cnt++] = BtnId;
        if(Cnt >= CODE_LEN) {
            if(IsCorrect()) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdCorrectCode));
            else EvtQMain.SendNowOrExit(EvtMsg_t(evtIdBadCode));
            Cnt = 0;
            Timeout = 0;
        }
        else {
            Timeout = CODE_TIMEOUT_S;
            EvtQMain.SendNowOrExit(EvtMsg_t(evtIdSomeButton));
        }
    }

    void OnOneSecond() {
        if(Timeout > 0) {
            Timeout--;
            if(Timeout <= 0) {
                Cnt = 0; // Reset counter
                Printf("timeout\r");
            }
        }
    }

} CodeChecker;
#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk216MHz();
//    Clk.SetCoreClk80MHz();
    Clk.Setup48Mhz();
    Clk.SetPllSai1RDiv(3, 8); // SAI R div = 3 => R = 2*96/3 = 64 MHz; LCD_CLK = 64 / 8 = 8MHz
    // SAI clock: PLLSAI1 Q
    Clk.SetPllSai1QDiv(8, 1); // Q = 2 * 96 / 8 = 24; 24/1 = 24
    Clk.SetSai2ClkSrc(saiclkPllSaiQ);
    FLASH->ACR |= FLASH_ACR_ARTEN; // Enable ART accelerator
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    // ==== Init Hard & Soft ====
//    SdramInit();
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();
//    SwitchTo27MHz();

    Led.Init();
    Led.StartOrRestart(lsqIdle);

    SimpleSensors::Init();

#if 1
    SD.Init();

    // Audio codec
//    Codec.Init();   // i2c initialized inside, as pull-ups powered by VAA's LDO
//    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
//    Codec.DisableHeadphones();
//    Codec.EnableSpeakerMono();
//    Codec.SetupMonoStereo(Mono);  // Always
//    Codec.SetupSampleRate(22050); // Just default, will be replaced when changed

//    LcdInit();

//    Avi::Init();
//    Avi::Start("sw8_m.avi", 000);
//    Avi::Start("trailer_48000_0.avi ", 000);
#endif
//    Npx.Init();

    TmrOneSecond.StartOrRestart();

    // ==== Main cycle ====
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdShellCmd:
                Led.StartOrRestart(lsqCmd);
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdButtons:
                if(AppState != stateIdle) {
                    // TODO: stop
                }
                CodeChecker.OnBtnPress(Msg.BtnEvtInfo.BtnID);
                break;

            case evtIdSomeButton:
                Printf("  SomeBtn\r");
                break;

            case evtIdCorrectCode:
                Printf("  Correct\r");
                break;

            case evtIdBadCode:
                Printf("  Bad\r");
                break;

            case evtIdEverySecond:
                Printf("S\r");
                CodeChecker.OnOneSecond();
                break;

#if 0 // ======= USB =======
            case evtIdUsbConnect:
                Printf("USB connect\r");
                UsbMsdCdc.Connect();
                break;
            case evtIdUsbDisconnect:
                UsbMsdCdc.Disconnect();
                Printf("USB disconnect\r");
                break;
            case evtIdUsbReady:
                Printf("USB ready\r");
                break;
#endif
            default: break;
        } // switch
    } // while true
}

void SwitchTo27MHz() {
    chSysLock();
//    TMR_DISABLE(STM32_ST_TIM);          // Stop counter
    Clk.SetupBusDividers(ahbDiv8, apbDiv1, apbDiv1);
    Clk.SetupFlashLatency(27, 3300);
    Clk.UpdateFreqValues();
    chSysUnlock();
    Clk.PrintFreqs();
}

void SwitchTo216MHz() {
    chSysLock();
//    TMR_DISABLE(STM32_ST_TIM);          // Stop counter
    // APB1 is 54MHz max, APB2 is 108MHz max
    Clk.SetupFlashLatency(216, 3300);
    Clk.SetupBusDividers(ahbDiv1, apbDiv4, apbDiv2);
    Clk.UpdateFreqValues();
    chSysUnlock();
    Clk.PrintFreqs();
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    else if(PCmd->NameIs("mem")) PrintMemoryInfo();


    else if(PCmd->NameIs("clr")) {
        Color_t Clr;
        if(PCmd->GetNext<uint8_t>(&Clr.R) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.G) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.B) != retvOk) { PShell->Ack(retvCmdError); return; }
        for(int i=0; i<2; i++) Npx.ClrBuf[i] = Clr;
        Npx.SetCurrentColors();
    }

    else if(PCmd->NameIs("lcd")) {
        uint32_t A, R, G, B;
        if(PCmd->GetNext<uint32_t>(&A) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint32_t>(&R) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint32_t>(&G) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint32_t>(&B) != retvOk) { PShell->Ack(retvCmdError); return; }
        LcdPaintL1(0, 0, 99, 99, A, R, G, B);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("chk")) {
        SdramCheck();
    }

    else if(PCmd->NameIs("27")) {
        SwitchTo27MHz();
    }
    else if(PCmd->NameIs("216")) {
        SwitchTo216MHz();
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
