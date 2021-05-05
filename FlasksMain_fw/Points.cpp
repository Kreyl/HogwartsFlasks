/*
 * Points.cpp
 *
 *  Created on: 4 мая 2021 г.
 *      Author: layst
 */

#include "Points.h"
#include "color.h"
#include "ws2812b.h"
#include "kl_lib.h"
#include "MsgQ.h"
#include "EvtMsgIDs.h"

#if 1 // =============================== Defines ===============================
#define BCKP_REG_GRIF_INDX  11
#define BCKP_REG_SLYZ_INDX  12
#define BCKP_REG_RAVE_INDX  13
#define BCKP_REG_HUFF_INDX  14

#define FLASK_MAX_VALUE     1000L
#define FLASK_OFF_CLR       clBlack
#define EFF_LED2LED_DIST    27

#define MAX_FLARE_CNT       7
#define FLARE_MAX_VALUE     9L
#define FLARE_START_DELAY   22
#define FLARE_MIN_DELAY     3
#define FLARE_DIST_BETWEEN  18
#define MIN_DELAY_BETWEEN_FLARES    270

#endif

#if 1 // ============================ Msg q ====================================
enum PointCmd_t : uint8_t {
    pocmdNone, pocmdNewTarget, pocmdRescale, pocmdFlareTimer, pocmdCheckIfAddFlare,
    pocmdRefresh,
};

union PointMsg_t {
    uint32_t DWord[2];
    struct {
        PointCmd_t Cmd;
        union {
            int32_t Value;
            void *ptr;
        };
    } __packed;
    PointMsg_t& operator = (const PointMsg_t &Right) {
        DWord[0] = Right.DWord[0];
        DWord[1] = Right.DWord[1];
        return *this;
    }
    PointMsg_t() : Cmd(pocmdNone) {}
    PointMsg_t(PointCmd_t ACmd) : Cmd(ACmd) {}
    PointMsg_t(PointCmd_t ACmd, void *ptr) : Cmd(ACmd), ptr(ptr) {}
} __packed;

static EvtMsgQ_t<PointMsg_t, 45> MsgQPoints;
#endif

static int32_t FlaskMaxValue = FLASK_MAX_VALUE;

class Flask_t;

enum FlareType_t {flaretypeUp, flaretypeDown};

void FlareTmrCallback(void *p) {
    chSysLockFromISR();
    MsgQPoints.SendNowOrExitI(PointMsg_t(pocmdFlareTimer, p));
    chSysUnlockFromISR();
}

class Flare_t {
private:
    virtual_timer_t ITmr;
    int32_t TickCnt;
public:
    Flask_t *Parent = nullptr; // nullptr means idle
    int32_t CurrY = 0;
    FlareType_t Type = flaretypeDown;
    int32_t PointsCarried;
    uint32_t Delay_ms;

    bool IsIdle() { return (Parent == nullptr); }
    void StartTmr() { chVTSet(&ITmr, TIME_MS2I(Delay_ms), FlareTmrCallback, this); }
    void Start() {
        Delay_ms = FLARE_START_DELAY;
        TickCnt = 0;
        StartTmr();
    }
    void Draw();
    void Move();
};

class Flask_t {
private:
    int32_t StartIndx, EndIndx, LedCnt; // LEDs' IDs
    void SetColorAtIndx(int32_t x, Color_t AColor) {
        int32_t LedIndx = (StartIndx < EndIndx)? (StartIndx + x) : (StartIndx - x);
        if(LedIndx >= StartIndx and LedIndx <= EndIndx and LedIndx < NPX_LED_CNT) {
            Npx.ClrBuf[LedIndx] = AColor;
        }
    }
    Color_t Clr;

    Flare_t Flares[MAX_FLARE_CNT];
    Flare_t* FindIdleFlare() {
        for(uint32_t i=0; i<MAX_FLARE_CNT; i++) {
            if(Flares[i].IsIdle()) return &Flares[i];
        }
        return nullptr; // not found
    }
    int32_t FlareCnt = 0;
    int32_t CurrValueIndx = 0;
    systime_t FlareAddTime = 0;
public:
    friend class Flare_t;
    Flask_t (int32_t AStartID, int32_t AEndID, Color_t AColor, int32_t Indx) : StartIndx(AStartID), EndIndx(AEndID), Clr(AColor), SelfIndx(Indx) {
        LedCnt = (StartIndx < EndIndx) ? (1 + EndIndx - StartIndx) : (1 + StartIndx - EndIndx);
    }

    int32_t SelfIndx;
    int32_t TargetPoints = 0, DisplayedPoints = 0, PointsInFlight = 0;

    void Redraw() {
        // Redraw value
        CurrValueIndx = ((DisplayedPoints * LedCnt) / FlaskMaxValue);
        if(CurrValueIndx == 0 and DisplayedPoints > 0) CurrValueIndx = 1;
        if(CurrValueIndx < 0) CurrValueIndx = 0;
        if(StartIndx < EndIndx) { // Normal direction
            int32_t TargetLedID = StartIndx + CurrValueIndx;
            // Set LEDs on
            for(int32_t i=StartIndx; i<TargetLedID; i++) Npx.ClrBuf[i] = Clr;
            // Set LEDs off
            for(int32_t i=TargetLedID; i<=EndIndx; i++) Npx.ClrBuf[i] = FLASK_OFF_CLR;
        }
        else {
            int32_t TargetLedID = StartIndx - CurrValueIndx;
            // Set LEDs on
            for(int32_t i=StartIndx; i>TargetLedID; i--) Npx.ClrBuf[i] = Clr;
            // Set LEDs off
            for(int32_t i=TargetLedID; i>=EndIndx; i--) Npx.ClrBuf[i] = FLASK_OFF_CLR;
        }
        // Redraw flares
        int32_t cnt = FlareCnt;
        for(Flare_t& Flre : Flares) {
            if(cnt == 0) break;
            if(!Flre.IsIdle()) {
                Flre.Draw();
                cnt--;
            }
        }
    }

    void AddFlareIfNeeded() {
        int32_t PointsInFuture = DisplayedPoints + PointsInFlight;
        int32_t DeltaPoints = TargetPoints - PointsInFuture;
        if((PointsInFuture == TargetPoints) or (FlareCnt >= MAX_FLARE_CNT)) return;
        // Check if enough time passed
        if(FlareCnt != 0 and chVTTimeElapsedSinceX(FlareAddTime) < TIME_MS2I(MIN_DELAY_BETWEEN_FLARES)) return;
        // Add flare
        Flare_t* flre = FindIdleFlare();
        if(!flre) return;
        FlareCnt++;
        flre->Parent = this;
        FlareAddTime = chVTGetSystemTimeX();
        // Adding points
        if(DeltaPoints > 0) {
            flre->Type = flaretypeDown;
            flre->CurrY = LedCnt;
            if(DeltaPoints > FLARE_MAX_VALUE) flre->PointsCarried = FLARE_MAX_VALUE;
            else flre->PointsCarried = DeltaPoints;
            PointsInFlight += flre->PointsCarried;
        }
        // Removing points
        else {
            flre->Type = flaretypeUp;
            flre->CurrY = CurrValueIndx;
            if(DeltaPoints < -FLARE_MAX_VALUE) flre->PointsCarried = -FLARE_MAX_VALUE;
            else flre->PointsCarried = DeltaPoints;
            DisplayedPoints += flre->PointsCarried;
            Redraw();
            EvtQMain.SendNowOrExit(EvtMsg_t(evtIdPointsRemove, SelfIndx));
        }
//        Printf("Flre: Y%u\r", flre->CurrY);
        flre->Start();
    }
};

void Flare_t::Draw() {
    Parent->SetColorAtIndx(CurrY, Parent->Clr);
    if(Type == flaretypeDown) {
        Parent->SetColorAtIndx(CurrY+1, FLASK_OFF_CLR);
    }
    else {
        Parent->SetColorAtIndx(CurrY-1, FLASK_OFF_CLR);
    }
}

void Flare_t::Move() {
    if(Type == flaretypeDown) {
        CurrY--;
        if(CurrY <= Parent->CurrValueIndx) { // Our toil have passed
            Flask_t *PFlsk = Parent;
            PFlsk->FlareCnt--;
            Parent = nullptr; // make idle
            PFlsk->DisplayedPoints += PointsCarried;
            PFlsk->PointsInFlight -= PointsCarried;
            PFlsk->Redraw();
            MsgQPoints.SendNowOrExit(PointMsg_t(pocmdCheckIfAddFlare, PFlsk));
            EvtQMain.SendNowOrExit(EvtMsg_t(evtIdPointsAdd, PFlsk->SelfIndx));
            return;
        }
    }
    else {
        CurrY++;
        if(CurrY >= Parent->LedCnt) {
            Flask_t *PFlsk = Parent;
            PFlsk->FlareCnt--;
            Parent = nullptr; // make idle
            PFlsk->Redraw();
            MsgQPoints.SendNowOrExit(PointMsg_t(pocmdCheckIfAddFlare, PFlsk));
            return;
        }
    }
    // Did not reached bound, so draw it
    Draw();
    TickCnt++;
    // Adjust delay
    if(Delay_ms > FLARE_MIN_DELAY) {
        if(TickCnt % 4 == 0) Delay_ms--;
        if(TickCnt % FLARE_DIST_BETWEEN == 0) MsgQPoints.SendNowOrExit(PointMsg_t(pocmdCheckIfAddFlare, Parent));
    }
    StartTmr();
}

static Flask_t Flasks[4] = {
        {0, 128, clRed, INDX_GRIF},
        {129, 257, clGreen, INDX_SLYZE},
        {258, 386, clBlue, INDX_RAVE},
        {387, 515, {255, 200, 0}, INDX_HUFF},
};

#if 1 // =========================== Thread ====================================
static THD_WORKING_AREA(waPointsThread, 1024);

__noreturn
static void PointsThread(void *arg) {
    chRegSetThreadName("Points");
    while(true) {
        PointMsg_t Msg = MsgQPoints.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case pocmdNewTarget:
                for(Flask_t &Flask : Flasks) Flask.AddFlareIfNeeded();
                break;

            case pocmdRescale:
                for(Flask_t &Flask : Flasks) Flask.Redraw();
                break;

            case pocmdCheckIfAddFlare:
                ((Flask_t*)Msg.ptr)->AddFlareIfNeeded();
                break;

            case pocmdFlareTimer:
                ((Flare_t*)Msg.ptr)->Move();
                break;

            case pocmdRefresh:
                Npx.SetCurrentColors();
                break;

            default: break;
        } // switch
    } // while true
}
#endif

static void OnTransmitEndI() {
    MsgQPoints.SendNowOrExitI(PointMsg_t(pocmdRefresh));
}

namespace Points {

void Init() {
//    ITmr.StartOrRestart();
    // Create thread
    MsgQPoints.Init();
    chThdCreateStatic(waPointsThread, sizeof(waPointsThread), NORMALPRIO, PointsThread, NULL);
    // Npx must refresh always
    Npx.OnTransmitEnd = OnTransmitEndI;
    Npx.SetCurrentColors();
    // Load current values
    int32_t G, S, R, H;
    G = BackupSpc::ReadRegister(BCKP_REG_GRIF_INDX);
    S = BackupSpc::ReadRegister(BCKP_REG_SLYZ_INDX);
    R = BackupSpc::ReadRegister(BCKP_REG_RAVE_INDX);
    H = BackupSpc::ReadRegister(BCKP_REG_HUFF_INDX);
    Set(G, S, R, H);
}

void Set(int32_t AGrif, int32_t ASlyze, int32_t ARave, int32_t AHuff) {
    // Check if changed
    if(     (Flasks[INDX_GRIF].TargetPoints == AGrif) and
            (Flasks[INDX_SLYZE].TargetPoints == ASlyze) and
            (Flasks[INDX_RAVE].TargetPoints == ARave) and
            (Flasks[INDX_HUFF].TargetPoints == AHuff)) return;

    // Store in Backup Regs
    BackupSpc::WriteRegister(BCKP_REG_GRIF_INDX, AGrif);
    BackupSpc::WriteRegister(BCKP_REG_SLYZ_INDX, ASlyze);
    BackupSpc::WriteRegister(BCKP_REG_RAVE_INDX, ARave);
    BackupSpc::WriteRegister(BCKP_REG_HUFF_INDX, AHuff);

    // Set target values immediately
    Flasks[INDX_GRIF].TargetPoints = AGrif;
    Flasks[INDX_SLYZE].TargetPoints = ASlyze;
    Flasks[INDX_RAVE].TargetPoints = ARave;
    Flasks[INDX_HUFF].TargetPoints = AHuff;

    // Calculate scaling. Find max, min is always 0
    int32_t max = FLASK_MAX_VALUE;
    if(AGrif  > max) max = AGrif;
    if(ASlyze > max) max = ASlyze;
    if(ARave  > max) max = ARave;
    if(AHuff  > max) max = AHuff;
    if(max != FlaskMaxValue) {
        FlaskMaxValue = max;
        MsgQPoints.SendNowOrExit(PointMsg_t(pocmdRescale));
    }

    // Set them
    MsgQPoints.SendNowOrExit(PointMsg_t(pocmdNewTarget));
    EvtQMain.SendNowOrExit(EvtMsg_t(evtIdPointsSet));
}

void Print() {
    Printf("%d %d %d %d / %d %d %d %d\r",
            Flasks[INDX_GRIF].TargetPoints,
            Flasks[INDX_SLYZE].TargetPoints,
            Flasks[INDX_RAVE].TargetPoints,
            Flasks[INDX_HUFF].TargetPoints,

            Flasks[INDX_GRIF].DisplayedPoints,
            Flasks[INDX_SLYZE].DisplayedPoints,
            Flasks[INDX_RAVE].DisplayedPoints,
            Flasks[INDX_HUFF].DisplayedPoints
    );
}

} // namespace
