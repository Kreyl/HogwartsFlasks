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
#define FLASK_MAX_VALUE     1000L
#define FLASK_MAX_INCREMENT 500L
#define FLASK_OFF_CLR       clBlack
#define EFF_LED2LED_DIST    27

#define MAX_FLARE_CNT       7
#define FLARE_MAX_VALUE     9L
#define FLARE_START_DELAY   22
#define FLARE_MIN_DELAY     3
#define FLARE_DIST_BETWEEN  18
#define MIN_DELAY_BETWEEN_FLARES    270

static const int32_t kwandering_flares_cnt = 7;
static const Color_t kwandering_flare_clr{255, 255, 255};
static const int32_t ksmooth_value_min = 180L;
static const int32_t ksmooth_value_max = 360L;

#endif

#if 1 // ============================ Msg q ====================================
enum PointCmd_t : uint8_t {
    pocmdNone, pocmdNewTarget, pocmdSetNow, pocmdRescale, pocmdFlareTimer, pocmdCheckIfAddFlare,
    pocmdRefresh, pocmdHide, pocmdShow,
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
    PointMsg_t(PointCmd_t ACmd, void *aptr) : Cmd(ACmd), ptr(aptr) {}
} __packed;

static EvtMsgQ_t<PointMsg_t, 45> MsgQPoints;
#endif

static int32_t flask_max_value = FLASK_MAX_VALUE;
static bool points_are_hidden = false;

class Flask_t;

void FlareTmrCallback(void *p) {
    chSysLockFromISR();
    MsgQPoints.SendNowOrExitI(PointMsg_t(pocmdFlareTimer, p));
    chSysUnlockFromISR();
}

class Flare {
private:
    virtual_timer_t itmr;
    int32_t tick_cnt;
    void AdjustDelayForUpDown() {
        tick_cnt++;
        if(delay_ms > FLARE_MIN_DELAY) {
            if(tick_cnt % 4 == 0) delay_ms--;
            if(tick_cnt % FLARE_DIST_BETWEEN == 0) MsgQPoints.SendNowOrExit(PointMsg_t(pocmdCheckIfAddFlare, parent));
        }
    }
public:
    enum class Type {Up, Down, Wandering};
    Flask_t *parent = nullptr; // nullptr means idle
    int32_t curr_y = 0;
    Type type = Type::Down;
    int32_t points_carried;
    uint32_t delay_ms;
    // For wandering
    Color_t curr_clr, target_clr;
    int32_t smooth_value;

    bool IsIdle() { return (parent == nullptr); }
    void StartTmr() { chVTSet(&itmr, TIME_MS2I(delay_ms), FlareTmrCallback, this); }
    void Start() {
        delay_ms = FLARE_START_DELAY;
        tick_cnt = 0;
        StartTmr();
    }
    void Draw();
    void Move();
    void StopI() {
        chVTResetI(&itmr);
        parent = nullptr;
    }
};

class Flask_t {
private:
    int32_t start_indx, end_indx, led_cnt; // LEDs' IDs
    void SetColorAtIndx(int32_t x, Color_t AColor) {
        int32_t LedIndx = start_indx + x;
        if(LedIndx >= start_indx and LedIndx <= end_indx and LedIndx < NPX_LED_CNT) {
            Npx.ClrBuf[LedIndx] = AColor;
        }
    }
    Color_t clr;

    Flare flares[MAX_FLARE_CNT];
    Flare* FindIdleFlare() {
        for(uint32_t i=0; i<MAX_FLARE_CNT; i++) {
            if(flares[i].IsIdle()) return &flares[i];
        }
        return nullptr; // not found
    }
    int32_t flare_cnt = 0;
    int32_t curr_value_indx = 0;
    systime_t flare_add_time = 0;
public:
    friend class Flare;
    Flask_t (int32_t AStartID, int32_t AEndID, Color_t AColor, int32_t Indx) : start_indx(AStartID), end_indx(AEndID), clr(AColor), self_indx(Indx) {
        led_cnt = 1 + end_indx - start_indx;
    }

    int32_t self_indx;
    int32_t target_points = 0, displayed_points = 0, points_in_flight = 0;

    void Redraw() {
        // Redraw value
        if(points_are_hidden) {
            for(int32_t i=start_indx; i<=end_indx; i++) Npx.ClrBuf[i] = FLASK_OFF_CLR; // All LEDs off
        }
        else {  // Points are not hidden
            curr_value_indx = (displayed_points * led_cnt) / flask_max_value;
            if(curr_value_indx == 0 and displayed_points > 0) curr_value_indx = 1;
            else if(curr_value_indx < 0) curr_value_indx = 0;
            else if(curr_value_indx >= led_cnt) curr_value_indx = led_cnt - 1;
            int32_t target_led_indx = start_indx + curr_value_indx;
            // Set LEDs on
            for(int32_t i=start_indx; i<target_led_indx; i++) Npx.ClrBuf[i] = clr;
            // Set LEDs off
            for(int32_t i=target_led_indx; i<=end_indx; i++) Npx.ClrBuf[i] = FLASK_OFF_CLR;
        }
        // Redraw flares
        int32_t cnt = flare_cnt;
        for(Flare& flre : flares) {
            if(cnt == 0) break;
            if(!flre.IsIdle()) {
                flre.Draw();
                cnt--;
            }
        }
    }

    void AddPointFlareIfNeeded() {
        int32_t PointsInFuture = displayed_points + points_in_flight;
        int32_t DeltaPoints = target_points - PointsInFuture;
        if((PointsInFuture == target_points) or (flare_cnt >= MAX_FLARE_CNT)) return;
        // Check if enough time passed
        if(flare_cnt != 0 and chVTTimeElapsedSinceX(flare_add_time) < TIME_MS2I(MIN_DELAY_BETWEEN_FLARES)) return;
        // Add flare
        Flare* flre = FindIdleFlare();
        if(!flre) return;
        flare_cnt++;
        flre->parent = this;
        flare_add_time = chVTGetSystemTimeX();
        // Adding points
        if(DeltaPoints > 0) {
            flre->type = Flare::Type::Down;
            flre->curr_y = led_cnt;
            if(DeltaPoints > FLARE_MAX_VALUE) flre->points_carried = FLARE_MAX_VALUE;
            else flre->points_carried = DeltaPoints;
            points_in_flight += flre->points_carried;
        }
        // Removing points
        else {
            flre->type = Flare::Type::Up;
            flre->curr_y = curr_value_indx;
            if(DeltaPoints < -FLARE_MAX_VALUE) flre->points_carried = -FLARE_MAX_VALUE;
            else flre->points_carried = DeltaPoints;
            displayed_points += flre->points_carried;
            Redraw();
            EvtQMain.SendNowOrExit(EvtMsg_t(evtIdPointsRemove, self_indx));
        }
        flre->Start();
    }

    void AddWanderingFlare() {
        Flare* flre = FindIdleFlare();
        if(!flre) return;
        flare_cnt++;
        flre->parent = this;
        flre->type = Flare::Type::Wandering;
        RestartWanderingFlare(flre);
    }

    void RestartWanderingFlare(Flare* pflre) {
        bool uniq;
        do {
            pflre->curr_y = Random::Generate(0, led_cnt);
            uniq = true;
            for(Flare& flre : flares) {
                if(&flre == pflre or flre.IsIdle()) continue;
                if(flre.curr_y == pflre->curr_y) {
                    uniq = false;
                    break;
                }
            }
        } while(!uniq);
        pflre->curr_clr = clBlack;
        pflre->target_clr = kwandering_flare_clr;
        pflre->smooth_value = Random::Generate(ksmooth_value_min, ksmooth_value_max);
        pflre->Start();
    }

    void StopFlares() {
        chSysLock();
        for(Flare& Flre : flares) Flre.StopI();
        flare_cnt = 0;
        points_in_flight = 0;
        flare_add_time = 0;
        chSysUnlock();
    }
};

static Flask_t flasks[4] = {
        {0, 128, clRed, INDX_GRIF},
        {129, 257, clGreen, INDX_SLYZE},
        {258, 386, clBlue, INDX_RAVE},
        {387, 515, {255, 200, 0}, INDX_HUFF},
};

void Flare::Draw() {
    switch(type) {
        case Type::Down:
            parent->SetColorAtIndx(curr_y, parent->clr); // draw flash
            parent->SetColorAtIndx(curr_y+1, FLASK_OFF_CLR); // clear prev position
            break;
        case Type::Up:
            parent->SetColorAtIndx(curr_y, parent->clr); // draw flash
            parent->SetColorAtIndx(curr_y-1, FLASK_OFF_CLR); // clear prev position
            break;
        case Type::Wandering:
            parent->SetColorAtIndx(curr_y, curr_clr);
            break;
    } // switch
}

void Flare::Move() {
    if(IsIdle()) return;
    switch(type) {
        case Type::Down:
            curr_y--;
            if(curr_y <= parent->curr_value_indx) { // Our toil have passed
                Flask_t *PFlsk = parent;
                PFlsk->flare_cnt--;
                parent = nullptr; // make idle
                PFlsk->displayed_points += points_carried;
                PFlsk->points_in_flight -= points_carried;
                PFlsk->Redraw();
                MsgQPoints.SendNowOrExit(PointMsg_t(pocmdCheckIfAddFlare, PFlsk));
                EvtQMain.SendNowOrExit(EvtMsg_t(evtIdPointsAdd, PFlsk->self_indx));
                return;
            }
            else AdjustDelayForUpDown();
            break;
        case Type::Up:
            curr_y++;
            if(curr_y >= parent->led_cnt) {
                Flask_t *PFlsk = parent;
                PFlsk->flare_cnt--;
                parent = nullptr; // make idle
                PFlsk->Redraw();
                MsgQPoints.SendNowOrExit(PointMsg_t(pocmdCheckIfAddFlare, PFlsk));
                return;
            }
            else AdjustDelayForUpDown();
            break;
        case Type::Wandering:
            delay_ms = curr_clr.DelayToNextAdj(target_clr, smooth_value);
            if(delay_ms == 0) { // colors are equal
                if(target_clr == clBlack) { // fadeout completed
                    parent->RestartWanderingFlare(this);
                    return;
                }
                else { // fadein completed
                    target_clr = clBlack;
                    delay_ms = curr_clr.DelayToNextAdj(target_clr, smooth_value);
                }
            }
            curr_clr.Adjust(target_clr);
            break;
    } // switch
    Draw();
    StartTmr();
}

#if 1 // =========================== Thread ====================================
static THD_WORKING_AREA(waPointsThread, 1024);

__noreturn
static void PointsThread(void *arg) {
    chRegSetThreadName("Points");
    while(true) {
        PointMsg_t Msg = MsgQPoints.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case pocmdNewTarget:
                for(Flask_t &flask : flasks) flask.AddPointFlareIfNeeded();
                break;

            case pocmdSetNow:
                for(Flask_t &flask : flasks) {
                    flask.StopFlares();
                    flask.displayed_points = flask.target_points;
                    flask.Redraw();
                }
                break;

            case pocmdRescale:
                for(Flask_t &flask : flasks) flask.Redraw();
                break;

            case pocmdCheckIfAddFlare:
                ((Flask_t*)Msg.ptr)->AddPointFlareIfNeeded();
                break;

            case pocmdHide:
                points_are_hidden = true;
                for(Flask_t &flask : flasks) {
                    flask.StopFlares();
                    flask.displayed_points = 0;
                    flask.Redraw();
                    for(int32_t i=0; i<kwandering_flares_cnt; i++) flask.AddWanderingFlare();
                }
                break;

            case pocmdShow:
                points_are_hidden = false;
                for(Flask_t &flask : flasks) {
                    flask.StopFlares();
                    flask.displayed_points = 0;
                    flask.Redraw();
                    flask.AddPointFlareIfNeeded();
                }
                break;

            case pocmdFlareTimer:
                ((Flare*)Msg.ptr)->Move();
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
    MsgQPoints.Init();
    chThdCreateStatic(waPointsThread, sizeof(waPointsThread), NORMALPRIO, PointsThread, NULL);
    // Npx must refresh always
    Npx.OnTransmitEnd = OnTransmitEndI;
    Npx.SetCurrentColors();
    // Load current values
    Values v;
    v.grif = BackupSpc::ReadRegister(BCKP_REG_GRIF_INDX);
    v.slyze = BackupSpc::ReadRegister(BCKP_REG_SLYZ_INDX);
    v.rave = BackupSpc::ReadRegister(BCKP_REG_RAVE_INDX);
    v.huff = BackupSpc::ReadRegister(BCKP_REG_HUFF_INDX);
    Set(v);
}

static int32_t ProcessValuesAndReturnMax(Values v) {
    // Store in Backup Regs
    BackupSpc::WriteRegister(BCKP_REG_GRIF_INDX, v.grif);
    BackupSpc::WriteRegister(BCKP_REG_SLYZ_INDX, v.slyze);
    BackupSpc::WriteRegister(BCKP_REG_RAVE_INDX, v.rave);
    BackupSpc::WriteRegister(BCKP_REG_HUFF_INDX, v.huff);
    // Save target values immediately
    flasks[INDX_GRIF].target_points = v.grif;
    flasks[INDX_SLYZE].target_points = v.slyze;
    flasks[INDX_RAVE].target_points = v.rave;
    flasks[INDX_HUFF].target_points = v.huff;
    // Calculate scaling. Find max, min is always 0
    int32_t max = v.Max();
    // Construct new max value: 1000, 1500, 2000, 2500...
    max = (1 + (max / FLASK_MAX_INCREMENT)) * FLASK_MAX_INCREMENT;
    if(max < FLASK_MAX_VALUE) max = FLASK_MAX_VALUE;
    return max;
}

void Set(Values v) {
    // Check if changed
    if(     (flasks[INDX_GRIF].target_points == v.grif) and
            (flasks[INDX_SLYZE].target_points == v.slyze) and
            (flasks[INDX_RAVE].target_points == v.rave) and
            (flasks[INDX_HUFF].target_points == v.huff)) return;

    int32_t max = ProcessValuesAndReturnMax(v);
    if(points_are_hidden) return; // Just put new target
    // Rescale if needed
    if(max != flask_max_value) {
        flask_max_value = max;
        MsgQPoints.SendNowOrExit(PointMsg_t(pocmdRescale));
    }
    // Set them
    MsgQPoints.SendNowOrExit(PointMsg_t(pocmdNewTarget));
    EvtQMain.SendNowOrExit(EvtMsg_t(evtIdPointsSet));
}

void SetNow(Values v) {
    flask_max_value = ProcessValuesAndReturnMax(v);
    if(points_are_hidden) return;
    MsgQPoints.SendNowOrExit(PointMsg_t(pocmdSetNow)); // Set them
}

Values GetDisplayed() {
    Values v;
    v.grif = flasks[INDX_GRIF].displayed_points;
    v.slyze = flasks[INDX_SLYZE].displayed_points;
    v.rave = flasks[INDX_RAVE].displayed_points;
    v.huff = flasks[INDX_HUFF].displayed_points;
    return v;
}

void Print() {
    Printf("%d %d %d %d / %d %d %d %d\r",
            flasks[INDX_GRIF].target_points,
            flasks[INDX_SLYZE].target_points,
            flasks[INDX_RAVE].target_points,
            flasks[INDX_HUFF].target_points,

            flasks[INDX_GRIF].displayed_points,
            flasks[INDX_SLYZE].displayed_points,
            flasks[INDX_RAVE].displayed_points,
            flasks[INDX_HUFF].displayed_points
    );
}

void Hide() { MsgQPoints.SendNowOrExit(PointMsg_t(pocmdHide)); }
void Show() { MsgQPoints.SendNowOrExit(PointMsg_t(pocmdShow)); }


} // namespace
