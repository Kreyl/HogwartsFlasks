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

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

#define NPX1_LED_CNT    3 //516
static const NeopixelParams_t LedParams(NPX1_SPI, NPX1_GPIO, NPX1_PIN, NPX1_AF, NPX1_DMA, NPX_DMA_MODE(NPX1_DMA_CHNL));
//static const NeopixelParams_t LedParams2(NPX2_SPI, NPX2_GPIO, NPX2_PIN, NPX2_AF, NPX2_DMA, NPX_DMA_MODE(NPX2_DMA_CHNL));
Neopixels_t Npx(&LedParams);


static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

LedBlinker_t Led{LED_PIN};
#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk80MHz();
    Clk.Setup48Mhz();
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    // ==== Init Hard & Soft ====
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    Led.Init();
    Led.StartOrRestart(lsqIdle);

    SD.Init();
    if(SD.IsReady) {
    }

//    PinSetupOut(GPIOF,  9, omPushPull);
//    PinSetHi(GPIOF, 9);
    // Npx
    Npx.Init(NPX1_LED_CNT);

//    TmrOneSecond.StartOrRestart();

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
//                Printf("Second\r");
                break;

            default: break;
        } // switch
    } // while true
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
    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
