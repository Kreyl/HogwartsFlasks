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
#include "lcdtft.h"
#include "AviDecode.h"
#include "CS42L52.h"
#include "AuPlayer.h"

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
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};
static TmrKL_t TmrStandbyAudio {TIME_MS2I(4005), evtIdStandbyAudio, tktOneShot};

static enum AppState_t {stateIdle, stateAudio, stateVideo} AppState = stateIdle;

void InitAudio();
void DeinitAudio();
void InitVideo();
void DeinitVideo();

void PlayAudio(const char* AFName) {
    if(AppState == stateIdle) InitAudio();
    TmrStandbyAudio.Stop();
    AuPlayer.Play(AFName, spmSingle);
}

void PlayVideo(const char* AFName) {
    AuPlayer.Stop();
    chThdSleepMilliseconds(360);
    //    if(AuPlayer.IsPlayingNow())
//    if(Codec.IsTransmitting())
//        AuPlayer.WaitEnd();
    if(AppState != stateVideo) InitVideo();
    TmrStandbyAudio.Stop();
    if(Avi::Start(AFName) != retvOk) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdVideoPlayStop));
}

#endif

#if 1 // ================================ Code =================================
static const uint8_t Code[] = {7,3,5,1,2,6};

#define FILENAME2PLAY       "predmet.avi"

#define CODE_LEN            countof(Code)
#define BTN_CNT             8
#define ENTERED_MAX_LEN     9
#define CODE_TIMEOUT_S      4

static const char* BtnFnames[BTN_CNT] = {"b1.wav", "b2.wav", "b3.wav", "b4.wav", "b5.wav", "b6.wav", "b7.wav", "b8.wav",};

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
    bool IsPlayingBadCode = false;
public:
    void OnBtnPress(uint8_t BtnId) {
        Printf("Btn %u\r", BtnId);
        if(AuPlayer.IsPlayingNow()) {
//            AuPlayer.WaitEnd();
            AuPlayer.Stop();
        }
        if(IsPlayingBadCode) {
            IsPlayingBadCode = false;
            return;
        }

        PlayAudio(BtnFnames[BtnId]);
        WhatEntered[Cnt++] = BtnId;
        if(Cnt >= CODE_LEN) {
            if(IsCorrect()) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdCorrectCode));
            else {
//                EvtQMain.SendNowOrExit(EvtMsg_t(evtIdBadCode));
                PlayAudio("lighting2.wav");
                IsPlayingBadCode = true;
            }
            Cnt = 0;
            Timeout = 0;
        }
        else Timeout = CODE_TIMEOUT_S;
    }

    void OnOneSecond() {
        if(Timeout > 0) {
            Timeout--;
            if(Timeout <= 0) {
                Cnt = 0; // Reset counter
                if(!AuPlayer.IsPlayingNow()) PlayAudio("I_Error.wav");
//                Printf("timeout\r");
            }
        }
    }

} CodeChecker;
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
    AuPlayer.Init();

    DeinitVideo();

    TmrOneSecond.StartOrRestart();
    PlayAudio("alive.wav");

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
                if(AppState == stateVideo) {
                    Avi::Stop();
                    chThdSleepMilliseconds(4);
                }
                else CodeChecker.OnBtnPress(Msg.BtnEvtInfo.BtnID);
                break;

            case evtIdCorrectCode:
                Printf("  Correct\r");
                PlayVideo(FILENAME2PLAY);
                break;

//            case evtIdBadCode:
//                Printf("  Bad\r");
//                PlayVideo("lighting2.avi");
//                break;

            case evtIdEverySecond:
                Iwdg::Reload();
//                Printf("S\r");
                CodeChecker.OnOneSecond();
                break;

            case evtIdStandbyVideo: DeinitVideo(); break;
            case evtIdStandbyAudio: DeinitAudio(); break;

            case evtIdAudioPlayStop:
                Printf("AudioEnd\r");
                if(AppState != stateVideo) TmrStandbyAudio.StartOrRestart();
                break;
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


void InitAudio() {
    Printf("%S\r", __FUNCTION__);
    SD.Resume();
    // Audio codec
    Codec.Init();
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Stereo);  // For wav player
    Codec.SetupSampleRate(22050); // Just default, will be replaced when changed
    Codec.SetMasterVolume(0);
    Codec.SetSpeakerVolume(0); // max
    AppState = stateAudio;
}

void DeinitAudio() {
    Printf("%S\r", __FUNCTION__);
    Codec.Deinit();
    SD.Standby();
    AppState = stateIdle;
}

void InitVideo() {
    Printf("%S\r", __FUNCTION__);
    SwitchTo216MHz();
    SdramInit();
    Codec.Deinit();
    InitAudio();
    Codec.SetMasterVolume(9);
    Codec.SetupMonoStereo(Mono); // For AVI player
    LcdInit();
    Avi::Resume();
    AppState = stateVideo;
}

void DeinitVideo() {
    Printf("%S\r", __FUNCTION__);
    Avi::Standby();
    LcdDeinit();
    DeinitAudio();
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
        DeinitVideo();
    }
    else if(PCmd->NameIs("216")) {
        InitVideo();
    }

    else if(PCmd->NameIs("vi")) {
        PlayVideo(FILENAME2PLAY);
    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
