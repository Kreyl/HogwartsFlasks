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
CmdUart_t Uart{CmdUartParams};

static const UartParams_t RS485Params(19200, RS485_PARAMS);
HostUart485_t RS485{RS485Params, RS485_TXEN};

static void ITask();
static void OnCmd(Shell_t *PShell);

//static bool UsbPinWasHi = false;
LedBlinker_t Led{LED_PIN};
TmrKL_t TmrBckgStop{TIME_MS2I(7200), evtIdBckgStop, tktOneShot};
#endif

#if 1 // ========= Lume =========
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

    // Time
    BackupSpc::EnableAccess();
    ClrH.DWord32 = BackupSpc::ReadRegister(BCKP_REG_CLRH_INDX);
    ClrM.DWord32 = BackupSpc::ReadRegister(BCKP_REG_CLRM_INDX);
    InitMirilli();
    Time.Init();

    Sound.Init();
    Sound.SetupVolume(81);
    Sound.SetSlotVolume(BACKGROUND_SLOT, 2048);
    Sound.PlayAlive();
    chThdSleepMilliseconds(1535);
    uint32_t Volume = BackupSpc::ReadRegister(BCKP_REG_VOLUME_INDX);
    if(Volume > 100) Volume = 100;
    Sound.SetupVolume(Volume);

    // Points
    Npx.Init();
    Points::Init();

    RS485.Init();

    // USB
//    UsbMsdCdc.Init();
//    PinSetupInput(USB_DETECT_PIN, pudPullDown); // Usb detect pin

    // ==== Main cycle ====
    ITask();
}

void SendScreenCmd() {
    int32_t AGrif, ASlyze, ARave, AHuff;
    Points::GetDisplayed(&AGrif, &ASlyze, &ARave, &AHuff);
    int32_t Sum = AGrif + ASlyze + ARave + AHuff;
    RS485.SendBroadcast(0, 1, "Set", "%d %d %d %d %d", AGrif, ASlyze, ARave, AHuff, Sum);
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
//                Printf("Second\r");
                IndicateNewSecond();
//                RS485.SendBroadcast(6, 2, "Ping");
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

            // Points
            case evtIdPointsSet:
                Sound.PlayBackgroundIfNotYet();
                TmrBckgStop.StartOrRestart();
                break;
            case evtIdPointsAdd:
                Sound.PlayAdd(Msg.Value);
                Sound.PlayBackgroundIfNotYet();
                TmrBckgStop.StartOrRestart();
                SendScreenCmd();
                break;
            case evtIdPointsRemove:
                Sound.PlayRemove(Msg.Value);
                Sound.PlayBackgroundIfNotYet();
                TmrBckgStop.StartOrRestart();
                SendScreenCmd();
                break;

            case evtIdBckgStop:
                Sound.StopBackground();
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
    if(PCmd->NameIs("Ping")) PShell->Ok();
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    else if(PCmd->NameIs("mem")) PrintMemoryInfo();

    else if(PCmd->NameIs("Volume")) {
        uint8_t Volume;
        if(PCmd->GetNext<uint8_t>(&Volume) != retvOk) return;
        if(Volume > 100) Volume = 100;
        Sound.SetupVolume(Volume);
        BackupSpc::WriteRegister(BCKP_REG_VOLUME_INDX, Volume);
        PShell->Ok();
    }

    else if(PCmd->NameIs("clr")) {
        Color_t Clr;
        if(PCmd->GetNext<uint8_t>(&Clr.R) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.G) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.B) != retvOk) { PShell->CmdError(); return; }
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
        PShell->Ok();
    }

    // Time speed
    else if(PCmd->NameIs("Fast")) {
        Time.BeFast();
        PShell->Ok();
    }
    else if(PCmd->NameIs("Norm")) {
        Time.BeNormal();
        PShell->Ok();
    }

    // Clock colors
    else if(PCmd->NameIs("ClrH")) {
        Color_t Clr;
        if(PCmd->GetNext<uint8_t>(&Clr.R) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.G) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.B) != retvOk) { PShell->CmdError(); return; }
        ClrH = Clr;
        BackupSpc::WriteRegister(BCKP_REG_CLRH_INDX, Clr.DWord32);
        PShell->Ok();
    }
    else if(PCmd->NameIs("ClrM")) {
        Color_t Clr;
        if(PCmd->GetNext<uint8_t>(&Clr.R) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.G) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<uint8_t>(&Clr.B) != retvOk) { PShell->CmdError(); return; }
        ClrM = Clr;
        BackupSpc::WriteRegister(BCKP_REG_CLRM_INDX, Clr.DWord32);
        PShell->Ok();
    }
#endif

    else if(PCmd->NameIs("Set")) {
        int32_t Grif, Slyze, Rave, Huff;
        if(PCmd->GetNext<int32_t>(&Grif) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<int32_t>(&Slyze) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<int32_t>(&Rave) != retvOk) { PShell->CmdError(); return; }
        if(PCmd->GetNext<int32_t>(&Huff) != retvOk) { PShell->CmdError(); return; }
        Points::Set(Grif, Slyze, Rave, Huff);
        PShell->Ok();
    }

    else if(PCmd->NameIs("Get")) {
        Points::Print();
    }


    else if(PCmd->NameIs("help")) {
        PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
        Printf( "Volume <0..100>\r"
                "SetTime <Year> <Month> <Day> <H> <M>\r"
                "GetTime\r"
                "ClrH <R> <G> <B>  - color of hour miril\r"
                "ClrM <R> <G> <B>  - color of minute miril\r"
                "Set <Grif> <Slyze> <Rave> <Huff> - set points\r"
                "Get - get current points\r"
        );
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->CmdUnknown();
    }
}
#endif
