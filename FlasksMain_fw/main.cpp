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

    Clk.UpdateFreqValues();
    FLASH->ACR |= FLASH_ACR_ARTEN; // Enable ART accelerator
    // ==== Init OS ====
    halInit();
    chSysInit();
    // ==== Init Hard & Soft ====
    SdramInit();
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();
//    Printf("FLASH->ACR: %X\r", FLASH->ACR);

    Led.Init();
    Led.StartOrRestart(lsqIdle);

//    SdramCheck();
//    Lora.Init();

    SD.Init();

    // Audio codec
    Codec.Init();   // i2c initialized inside, as pull-ups powered by VAA's LDO
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Mono);  // Always
    Codec.SetupSampleRate(22050); // Just default, will be replaced when changed


    LcdInit();
//    LcdPaintL1(0, 0, 100, 100, 255, 0, 255, 0);

    Avi::Init();
//    Avi::Start("Plane_480x272.avi");
    Avi::Start("SWTrail.avi");
//    Avi::Start("yogSS.avi");
//    Avi::Start("sw8_m.avi", 000);
//        for(int i=0; i<1; i++) {
//            if(Avi::GetNextFrame() != retvOk) break;
//            Avi::ShowFrame();
//            chThdSleepMilliseconds(450);
//        }
//    }
//    Avi::Stop();

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

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
