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

#if 1 // ============================== Points =================================
#define BCKP_REG_GRIF_INDX  11
#define BCKP_REG_SLYZ_INDX  12
#define BCKP_REG_RAVE_INDX  13
#define BCKP_REG_HUFF_INDX  14

#define FLASK_MIN_VALUE     0L    // }
#define FLASK_MAX_VALUE     1000L // } This will be translated to LEDs; real points should be translated to this.
#define FLASK_LEN           (FLASK_MAX_VALUE - FLASK_MIN_VALUE)
#define FLASK_OFF_CLR       clBlack
#define EFF_LED2LED_DIST    27
class Flask_t {
private:
    int32_t StartIndx, EndIndx, CurrIndx = -1; // LEDs' IDs
    int32_t TotalLedCount() { return (StartIndx < EndIndx) ? (1 + EndIndx - StartIndx) : (1 + StartIndx - EndIndx); }
    Color_t Clr;
    int32_t CurrValue = 0;
    void SetColorAt(int32_t x, Color_t AColor) {
        int32_t LedIndx = (StartIndx < EndIndx)? (StartIndx + x) : (StartIndx - x);
        if(LedIndx >= StartIndx and LedIndx <= EndIndx and LedIndx < NPX_LED_CNT) {
            Npx.ClrBuf[LedIndx] = AColor;
        }
    }
    // Effect
    class {
    public:
        int32_t Count = 0, FirstIndx = 0;
        int32_t ValueModifier = 0;
        void SetCount(int32_t NewCount) {
            Count = NewCount;
            if(Count == 0) {
                Count = 1;
                ValueModifier = 0;
            }
            else ValueModifier = 1;
        }
    } Flash;
public:
    enum Eff_t {effNone, effAdd, effRemove} Eff = effNone;
    Flask_t (int32_t AStarID, int32_t AEndID, Color_t AColor) :
        StartIndx(AStarID), EndIndx(AEndID), Clr(AColor) {}
    // Value must be within [FLASK_MIN_VALUE; FLASK_MAX_VALUE]
    void SetNewValue(int32_t AValue) {
        if(AValue < FLASK_MIN_VALUE or AValue > FLASK_MAX_VALUE) {
            Printf("FlaskBadValue: %d\r", AValue);
            return;
        }
        if(AValue == CurrValue) return; // Nothing to do
        int32_t TargetIndx = ((AValue * TotalLedCount()) / FLASK_LEN) - 1;
        if(AValue > 0 and TargetIndx < 0) TargetIndx = 0;
        // Select effect
        if(AValue > CurrValue) {
            Eff = effAdd;
            Flash.SetCount(TargetIndx - CurrIndx);
            Flash.FirstIndx = TotalLedCount();
        }
        else {
            Eff = effRemove;
            Flash.SetCount(CurrIndx - TargetIndx);
            Flash.FirstIndx = CurrIndx;
        }
        CurrValue = AValue;
    }

    void SetNewValueNow(int32_t AValue) {
        if(AValue < FLASK_MIN_VALUE or AValue > FLASK_MAX_VALUE) {
            Printf("FlaskBadValue: %d\r", AValue);
            return;
        }
        // Do what needed depending on direction of chunk
        if(StartIndx < EndIndx) { // Normal direction
            int32_t TargetLedID = StartIndx + (AValue * TotalLedCount()) / FLASK_LEN;
            // Set LEDs on
            for(int32_t i=StartIndx; i<TargetLedID; i++) Npx.ClrBuf[i] = Clr;
            // Set LEDs off
            for(int32_t i=TargetLedID; i<=EndIndx; i++) Npx.ClrBuf[i] = FLASK_OFF_CLR;
        }
        else {
            int32_t TargetLedID = StartIndx - (AValue * TotalLedCount()) / FLASK_LEN;
            // Set LEDs on
            for(int32_t i=StartIndx; i>TargetLedID; i--) Npx.ClrBuf[i] = Clr;
            // Set LEDs off
            for(int32_t i=TargetLedID; i>=EndIndx; i--) Npx.ClrBuf[i] = FLASK_OFF_CLR;
        }
    }

    void OnTick() {
        if(Eff == effAdd) {
            // Move what moves by one pixel
            Flash.FirstIndx--;
            int32_t Indx = Flash.FirstIndx;
            for(int32_t i=0; i<Flash.Count; i++) {
                SetColorAt(Indx, Clr);
                SetColorAt(Indx+1, clBlack);
                Indx += EFF_LED2LED_DIST;
            }
            // Check if threshold touched
            if(Flash.FirstIndx == CurrIndx) {
                Flash.FirstIndx += EFF_LED2LED_DIST;
                Flash.Count--;
                if(Flash.Count == 0) Eff = effNone;
                CurrIndx += Flash.ValueModifier;
                SetColorAt(CurrIndx, Clr);
            }
        }
        else if(Eff == effRemove) {
            // Move what moves by one pixel
            Flash.FirstIndx++;
            int32_t Indx = Flash.FirstIndx;
            for(int32_t i=0; (i<Flash.Count and Indx > CurrIndx); i++) {
                SetColorAt(Indx, Clr);
                // Check if curr indx touched
                if(Indx == (CurrIndx+1)) {
                    if(Flash.ValueModifier != 0) {
                        SetColorAt(CurrIndx, clBlack);
                        CurrIndx--;
                    }
                }
                else SetColorAt(Indx-1, clBlack); // if Indx is not CurrIndx, then it definitely higher at least by 2.
                Indx -= EFF_LED2LED_DIST;
            }
            // Check if top reached
            if(Flash.FirstIndx == (EndIndx+1)) { // to fade top LED
                Flash.FirstIndx -= EFF_LED2LED_DIST;
                Flash.Count--;
                if(Flash.Count == 0) Eff = effNone;
            }
        }
    }
};

Flask_t
    FlaskGrif(0, 128, clRed),
    FlaskSlyze(129, 257, clGreen),
    FlaskRave(258, 386, clBlue),
    FlaskHuff(387, 515, {255, 200, 0});

class Points_t {
private:
    TmrKL_t ITmr {TIME_MS2I(18), evtIdPointsTick, tktPeriodic};
public:
    void SetNewValues(int32_t AGrif, int32_t ASlyze, int32_t ARave, int32_t AHuff) {
        // Store in Backup Regs
        BackupSpc::WriteRegister(BCKP_REG_GRIF_INDX, AGrif);
        BackupSpc::WriteRegister(BCKP_REG_SLYZ_INDX, ASlyze);
        BackupSpc::WriteRegister(BCKP_REG_RAVE_INDX, ARave);
        BackupSpc::WriteRegister(BCKP_REG_HUFF_INDX, AHuff);
        // Find max, min is always 0
        int32_t max = FLASK_MAX_VALUE;
        if(AGrif  > max) max = AGrif;
        if(ASlyze > max) max = ASlyze;
        if(ARave  > max) max = ARave;
        if(AHuff  > max) max = AHuff;
        // Process negative values
        if(AGrif  < 0) AGrif = 0;
        if(ASlyze < 0) ASlyze = 0;
        if(ARave  < 0) ARave = 0;
        if(AHuff  < 0) AHuff = 0;
        // Calculate normalized values
        // XXX Add command queue
        FlaskGrif.SetNewValue ((AGrif  * FLASK_MAX_VALUE) / max);
        FlaskSlyze.SetNewValue((ASlyze * FLASK_MAX_VALUE) / max);
        FlaskRave.SetNewValue ((ARave  * FLASK_MAX_VALUE) / max);
        FlaskHuff.SetNewValue ((AHuff  * FLASK_MAX_VALUE) / max);
    }
    void Init() {
        ITmr.StartOrRestart();
        int32_t G, S, R, H;
        G = BackupSpc::ReadRegister(BCKP_REG_GRIF_INDX);
        S = BackupSpc::ReadRegister(BCKP_REG_SLYZ_INDX);
        R = BackupSpc::ReadRegister(BCKP_REG_RAVE_INDX);
        H = BackupSpc::ReadRegister(BCKP_REG_HUFF_INDX);
        SetNewValues(G, S, R, H);
    }
    void OnTick() {
        FlaskGrif.OnTick();
        FlaskSlyze.OnTick();
        FlaskRave.OnTick();
        FlaskHuff.OnTick();
        Npx.SetCurrentColors();
    }
} Points;
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
    SdramInit();
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    Led.Init();
    Led.StartOrRestart(lsqIdle);

//    SdramCheck();
//    Lora.Init();

//    SD.Init();
//    if(SD.IsReady) {
//    }

    // Time
    BackupSpc::EnableAccess();
    ClrH.DWord32 = BackupSpc::ReadRegister(BCKP_REG_CLRH_INDX);
    ClrM.DWord32 = BackupSpc::ReadRegister(BCKP_REG_CLRM_INDX);
    InitMirilli();
    Time.Init();

    // Points
    Npx.Init();
    Points.Init();

    Lora.Init();

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

            case evtIdPointsTick: Points.OnTick(); break;

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
        Points.SetNewValues(Grif, Slyze, Rave, Huff);
        PShell->Ack(retvOk);
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
