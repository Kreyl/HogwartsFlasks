#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "uart.h"
#include "shell.h"
#include "led.h"
#include "Sequences.h"
#include "sdram.h"
#include "lcdtft.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

LedBlinker_t Led{LED_PIN};

#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk80MHz();
    Clk.Setup48Mhz(2); // SAI R div = 2 => R = 2*96/2 = 96 MHz
    Clk.SetSaiDivR(8); // LCD_CLK = 96 / 8 = 12MHz
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

    SdramInit();
    LcdInit();

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

    else if(PCmd->NameIs("Paint")) {
        uint32_t Left, Top, Right, Bottom;
        uint32_t A, R, G, B;
        if(PCmd->GetNext<uint32_t>(&Left) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&Top) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&Right) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&Bottom) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&A) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&R) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&G) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&B) != retvOk) { PShell->Ack(retvBadValue); return; }
        LcdPaintL1(Left, Top, Right, Bottom, A, R, G, B);
        PShell->Ack(retvOk);
    }


    else if(PCmd->NameIs("WR")) {
        uint32_t Addr, Value;
        if(PCmd->GetNext<uint32_t>(&Addr) != retvOk) { PShell->Ack(retvBadValue); return; }
        if(PCmd->GetNext<uint32_t>(&Value) != retvOk) { PShell->Ack(retvBadValue); return; }
        *(volatile uint32_t*)Addr = Value;
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("RD")) {
        uint32_t Addr, Value;
        if(PCmd->GetNext<uint32_t>(&Addr) != retvOk) { PShell->Ack(retvBadValue); return; }
        Value = *(volatile uint32_t*)Addr;
        Printf("%X\r", Value);
    }

    else if(PCmd->NameIs("chk")) {
        SdramCheck();
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
