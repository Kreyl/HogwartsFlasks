#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "uart.h"
#include "shell.h"
#include "led.h"
#include "Sequences.h"
#include "EvtMsgIDs.h"
#include "kl_sd.h"
#include "sdram.h"
#include "lcdtft.h"
#include "kl_fs_utils.h"
#include "ini_kl.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{CmdUartParams};

static const UartParams_t RS485Params(19200, RS485_PARAMS);
CmdUart485_t RS485{RS485Params, RS485_TXEN};

static void ITask();
static void OnCmd(Shell_t *PShell);

void ShowNumber(int32_t N);

#define LOAD_DIGS   TRUE // Disable it to make binary smaller

static int32_t SelfIndx = 0xFF; // bad one
static int32_t CurrN = 999999; // Bad one

LedBlinker_t Led{LED_PIN};
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};
#endif

FIL IFile;

void main() {
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
    SdramInit();
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

//    SdramCheck();

    Led.Init();
    Led.On();

    SD.Init();
    ini::ReadInt32("Screen.ini", "Main", "Indx", &SelfIndx);
    Printf("Self indx: %d\r", SelfIndx);

    LcdInit();
    Dma2d::Init();
    Dma2d::Cls(FrameBuf1);

#if LOAD_DIGS
    ShowNumber(SelfIndx);
#endif

    RS485.Init();
    TmrOneSecond.StartOrRestart();

    // ==== Main cycle ====
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdUartCmdRcvd:
                Led.StartOrRestart(lsqCmd);
                while(((CmdUart_t*)Msg.Ptr)->TryParseRxBuff() == retvOk) OnCmd((Shell_t*)((CmdUart_t*)Msg.Ptr));
                break;

            case evtIdEverySecond:
                Iwdg::Reload();
//                Printf("S\r");
                break;

            default: break;
        } // switch
    } // while true
}

#if LOAD_DIGS
#include "0.h"
#include "1.h"
#include "2.h"
#include "3.h"
#include "4.h"
#include "5.h"
#include "6.h"
#include "7.h"
#include "8.h"
#include "9.h"
#include "minus.h"

static const uint8_t* PDigTable[10] = {Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9};
static const uint32_t DigW[10] = {Digit0_W, Digit1_W, Digit2_W, Digit3_W, Digit4_W, Digit5_W, Digit6_W, Digit7_W, Digit8_W, Digit9_W};

void ShowNumber(int32_t N) {
    if(CurrN == N) return;
    CurrN = N;

    // Put to bounds
    if(N > 9999) N = 9999;
    if(N < -999) N = -999;

    // Process negative
    bool IsNegative = false;
    if(N < 0) {
        N = -N;
        IsNegative = true;
    }

    Dma2d::Cls(FrameBuf1);

    uint32_t Dig1000 = 0, Dig100 = 0, Dig10 = 0, Dig0 = 0;
    while(N >= 1000) {
        Dig1000++;
        N -= 1000;
    }
    while(N >= 100) {
        Dig100++;
        N -= 100;
    }
    while(N >= 10) {
        Dig10++;
        N -= 10;
    }
    while(N > 0) {
        Dig0++;
        N -= 1;
    }
//    Printf("%u %u %u %u\r", Dig1000, Dig100, Dig10, Dig0);

    bool Show1000 = (Dig1000 != 0);
    bool Show100 = (Dig100 != 0) or Show1000;
    bool Show10 = (Dig10 != 0) or Show100;

    int32_t W = DigW[Dig0];

    if(Show10)   W += DigW[Dig10];
    if(Show100)  W += DigW[Dig100];
    if(Show1000) W += DigW[Dig1000];
    if(IsNegative) W += Minus_W;

    int32_t x = (LCD_WIDTH - W) / 2;

    if(IsNegative) {
        Dma2d::WaitCompletion();
        Dma2d::CopyBufferRGB((void*)DigitMinus, FrameBuf1, x, 0, Minus_W, 272);
        x += Minus_W;
    }

    if(Show1000) {
        Dma2d::WaitCompletion();
        Dma2d::CopyBufferRGB((void*)PDigTable[Dig1000], FrameBuf1, x, 0, DigW[Dig1000], 272);
        x += DigW[Dig1000];
    }
    if(Show100) {
        Dma2d::WaitCompletion();
        Dma2d::CopyBufferRGB((void*)PDigTable[Dig100], FrameBuf1, x, 0, DigW[Dig100], 272);
        x += DigW[Dig100];
    }
    if(Show10) {
        Dma2d::WaitCompletion();
        Dma2d::CopyBufferRGB((void*)PDigTable[Dig10], FrameBuf1, x, 0, DigW[Dig10], 272);
        x += DigW[Dig10];
    }
    Dma2d::WaitCompletion();
    Dma2d::CopyBufferRGB((void*)PDigTable[Dig0], FrameBuf1, x, 0, DigW[Dig0], 272);
}
#else
void ShowNumber(int32_t N) {
    if(CurrN == N) return;
    CurrN = N;
    Printf("N: %d\r", N);
}
#endif

#include "ellipsis.h"

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ok();
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    else if(PCmd->NameIs("mem")) PrintMemoryInfo();

//    else if(PCmd->NameIs("chk")) {
//        SdramCheck();
//    }

    else if(PCmd->NameIs("cls")) {
        uint32_t R, G, B;
        if(PCmd->GetNext<uint32_t>(&R) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint32_t>(&G) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint32_t>(&B) != retvOk) { PShell->CmdError(); return; }
        Dma2d::Cls(FrameBuf1, R, G, B);
        PShell->Ok();
    }

    else if(PCmd->NameIs("Set")) {
        int32_t G, S, R, H, Summ;
        if(PCmd->GetNext<int32_t>(&G) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&S) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&R) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&H) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&Summ) != retvOk) return;
        Printf("Set %d %d %d %d; sum %d\r", G, S, R, H, Summ);
        if((G + S + R + H) == Summ) {
            if(SelfIndx == 0) ShowNumber(G);
            else if(SelfIndx == 1) ShowNumber(S);
            else if(SelfIndx == 2) ShowNumber(R);
            else if(SelfIndx == 3) ShowNumber(H);
        }
        else Printf("Sum Err\r");
    }

    else if(PCmd->NameIs("Hide")) {
        Dma2d::Cls(FrameBuf1);
        Dma2d::WaitCompletion();
        int32_t x = (LCD_WIDTH - ellipsis_W) / 2;
        int32_t y = (LCD_HEIGHT - ellipsis_H) / 2;
        Dma2d::CopyBufferRGB((void*)ellipsis, FrameBuf1, x, y, ellipsis_W, ellipsis_H);
    }

//    else if(PCmd->NameIs("Number")) {
//        uint32_t N;
//        if(PCmd->GetNext<uint32_t>(&N) != retvOk) { PShell->Ack(retvCmdError); return; }
//        ShowNumber(N);
//        PShell->Ack(retvOk);
//    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->CmdUnknown();
    }
}
#endif
