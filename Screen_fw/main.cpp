#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "led.h"
#include "kl_sd.h"
#include "kl_fs_utils.h"
#include "kl_time.h"
#include "kl_i2c.h"
//#include "usb_msdcdc.h"
#include "sdram.h"
#include "lcdtft.h"
#include "AviDecode.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

//static bool UsbPinWasHi = false;
LedBlinker_t Led{LED_PIN};
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

static enum AppState_t {stateIdle, stateVideo} AppState = stateIdle;

void InitVideo();
void DeinitVideo();


void PlayVideo(const char* AFName) {
    chThdSleepMilliseconds(360);
    //    if(AuPlayer.IsPlayingNow())
//    if(Codec.IsTransmitting())
//        AuPlayer.WaitEnd();
    if(AppState != stateVideo) InitVideo();
    if(Avi::Start(AFName) != retvOk) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdVideoPlayStop));
}

#endif

int main() {
    Iwdg::InitAndStart(4005);
    Iwdg::Reload();
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

    Led.Init();
    Led.StartOrRestart(lsqIdle);

    SimpleSensors::Init();
    SD.Init();
    Avi::Init();

    DeinitVideo();

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

            case evtIdEverySecond:
                Iwdg::Reload();
//                Printf("S\r");
                break;

            case evtIdStandbyVideo: DeinitVideo(); break;
            case evtIdVideoPlayStop:
                Printf("VideoEnd\r");
                DeinitVideo();
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
    Clk.SetupBusDividers(ahbDiv8, apbDiv1, apbDiv1);
    Clk.SetupFlashLatency(27, 3300);
    Clk.UpdateFreqValues();
    chSysUnlock();
    Clk.PrintFreqs();
}

void SwitchTo216MHz() {
    chSysLock();
    // APB1 is 54MHz max, APB2 is 108MHz max
    Clk.SetupFlashLatency(216, 3300);
    Clk.SetupBusDividers(ahbDiv1, apbDiv4, apbDiv2);
    Clk.UpdateFreqValues();
    chSysUnlock();
    Clk.PrintFreqs();
}

void InitVideo() {
    Printf("%S\r", __FUNCTION__);
    SwitchTo216MHz();
    SdramInit();
    LcdInit();
    Avi::Resume();
    AppState = stateVideo;
}

void DeinitVideo() {
    Printf("%S\r", __FUNCTION__);
    Avi::Standby();
    LcdDeinit();
    SdramDeinit();
    SwitchTo27MHz();
    AppState = stateIdle;
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    else if(PCmd->NameIs("mem")) PrintMemoryInfo();


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

    else if(PCmd->NameIs("vi")) {
//        PlayVideo(FILENAME2PLAY);
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
