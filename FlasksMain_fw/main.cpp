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
#include "Mirilli.h"
//#include "usb_msdcdc.h"
#include "sdram.h"
#include "Lora.h"
#include "lcdtft.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

//static bool UsbPinWasHi = false;
LedBlinker_t Led{LED_PIN};
#endif

FIL ifile;
uint8_t ibuf[256];

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk216MHz();
//    Clk.SetCoreClk80MHz();
    Clk.Setup48Mhz();
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

    Led.Init();
    Led.StartOrRestart(lsqIdle);

//    SdramCheck();
//    Lora.Init();

    SD.Init();
//    if(TryOpenFileRead("65.txt", &ifile) == retvOk) {
//        uint32_t Sz = 99;
//        if(f_read(&ifile, ibuf, Sz, &Sz) == FR_OK) {
//            Printf("%A\r", ibuf, Sz, ' ');
//        }
//        CloseFile(&ifile);
//    }

    LcdInit();
    LcdPaintL1(0, 0, 99, 99, 99, 99, 255, 99);

//    Npx.Init();
    // USB
//    UsbMsdCdc.Init();
//    PinSetupInput(USB_DETECT_PIN, pudPullDown); // Usb detect pin

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
//                // Check if USB connected/disconnected
//                if(PinIsHi(USB_DETECT_PIN) and !UsbPinWasHi) {
//                    UsbPinWasHi = true;
//                    EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbConnect));
//                }
//                else if(!PinIsHi(USB_DETECT_PIN) and UsbPinWasHi) {
//                    UsbPinWasHi = false;
//                    EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbDisconnect));
//                }
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
