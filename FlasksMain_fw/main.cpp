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
//#include "Lora.h"
#include "FlasksSnd.h"
#include "Points.h"

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

#if 1 // ========= Lume =========
#define BCKP_REG_CLRH_INDX  1
#define BCKP_REG_CLRM_INDX  2
static void IndicateNewSecond();
Color_t ClrH(0, 0, 255);
Color_t ClrM(0, 255, 0);

class Hypertime_t {
public:
    void ConvertFromTime() {
        // Hours
        int32_t FH = Time.Curr.H;
        if(FH > 11) FH -= 12;
        if(H != FH) {
            H = FH;
            NewH = true;
        }
        // Minutes
        int32_t S = Time.Curr.M * 60 + Time.Curr.S;
        int32_t FMin = S / 150;    // 150s in one hyperminute (== 2.5 minutes)
        if(M != FMin) {
            M = FMin;
            NewM = true;
        }
    }
    int32_t H, M;
    bool NewH = true, NewM = true;
} Hypertime;
#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk160MHz();
    Clk.Setup48Mhz();
//    Clk.SetPllSai1RDiv(3, 8); // SAI R div = 3 => R = 2*96/3 = 64 MHz; LCD_CLK = 64 / 8 = 8MHz
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

    // Debug LED
    Led.Init();
    Led.StartOrRestart(lsqIdle);

//    SdramCheck();
//    Lora.Init();

    SD.Init();
//    if(SD.IsReady) {
//    }

    Sound.Init();
    Sound.SetupVolume(81);
    Sound.PlayAlive();

    // Time
    BackupSpc::EnableAccess();
    ClrH.DWord32 = BackupSpc::ReadRegister(BCKP_REG_CLRH_INDX);
    ClrM.DWord32 = BackupSpc::ReadRegister(BCKP_REG_CLRM_INDX);
    InitMirilli();
    Time.Init();

    // Points
    Npx.Init();
    Points::Init();

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
                IndicateNewSecond();
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

#if 1 // ============================== Lume ===================================
void IndicateNewSecond() {
    Time.GetDateTime();
    Hypertime.ConvertFromTime();
//    Printf("HyperH: %u; HyperM: %u;   ", Hypertime.H, Hypertime.M);
    ResetColorsToOffState(ClrH, ClrM);

    SetTargetClrH(Hypertime.H, ClrH);
    if(Hypertime.M == 0) {
        SetTargetClrM(0, ClrM);
        SetTargetClrM(11, ClrM);
    }
    else {
        uint32_t N = Hypertime.M / 2;
        if(Hypertime.M & 1) { // Odd, single
            SetTargetClrM(N, ClrM);
        }
        else { // Even, couple
            SetTargetClrM(N, ClrM);
            SetTargetClrM(N-1, ClrM);
        }
    }
    WakeMirilli();
}
#endif

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

#if 1 // Lume
    else if(PCmd->NameIs("GetTime")) {
        Time.GetDateTime();
        Time.Curr.Print();
    }
    else if(PCmd->NameIs("SetTime")) {
        DateTime_t dt = Time.Curr;
        if(PCmd->GetNext<int32_t>(&dt.Year) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&dt.Month) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&dt.Day) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&dt.H) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&dt.M) != retvOk) return;
        Time.Curr = dt;
        Time.SetDateTime();
        IndicateNewSecond();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Fast")) {
        Time.BeFast();
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Norm")) {
        Time.BeNormal();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("ClrH")) {
        Color_t Clr;
        if(PCmd->GetNext<uint8_t>(&Clr.R) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.G) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.B) != retvOk) { PShell->Ack(retvCmdError); return; }
        ClrH = Clr;
        BackupSpc::WriteRegister(BCKP_REG_CLRH_INDX, Clr.DWord32);
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("ClrM")) {
        Color_t Clr;
        if(PCmd->GetNext<uint8_t>(&Clr.R) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.G) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.B) != retvOk) { PShell->Ack(retvCmdError); return; }
        ClrM = Clr;
        BackupSpc::WriteRegister(BCKP_REG_CLRM_INDX, Clr.DWord32);
        PShell->Ack(retvOk);
    }
#endif

    else if(PCmd->NameIs("Set")) {
        int32_t Grif, Slyze, Rave, Huff;
        if(PCmd->GetNext<int32_t>(&Grif) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<int32_t>(&Slyze) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<int32_t>(&Rave) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<int32_t>(&Huff) != retvOk) { PShell->Ack(retvCmdError); return; }
        Points::Set(Grif, Slyze, Rave, Huff);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Get")) {
        Points::Print();
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
