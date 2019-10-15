/*
 * Mirilli.cpp
 *
 *  Created on: 30 сент. 2017 г.
 *      Author: Kreyl
 */

#include "kl_lib.h"
#include "Mirilli.h"
#if MIRILLI_USE_SK6812
#include "sk6812"
#else
#include "ws2812b.h"
#endif

extern Neopixels_t LumeLeds;
static thread_reference_t PThd = nullptr;

// Target colors
static Color_t ITargetClr[MIRILLI_LED_CNT];
static bool ColorsDone = true;

static uint32_t ICalcDelayN(uint32_t n, uint32_t SmoothValue) {
    return LumeLeds.ClrBuf[n].DelayToNextAdj(ITargetClr[n], SmoothValue);
}

static THD_WORKING_AREA(waMirilli, 256);
__noreturn
static void MirilliThread(void *arg) {
    chRegSetThreadName("Mirilli");
    while(true) {
        if(ColorsDone) {
            chSysLock();
            chThdSuspendS(&PThd);
            chSysUnlock();
        }
        else {
            uint32_t Delay = 0;
            for(int32_t i=0; i<MIRILLI_LED_CNT; i++) {
                uint32_t tmp = ICalcDelayN(i, SMOOTH_VALUE);  // }
                if(tmp > Delay) Delay = tmp;                  // } Calculate Delay
                LumeLeds.ClrBuf[i].Adjust(ITargetClr[i]);    // Adjust current color
            } // for
            LumeLeds.SetCurrentColors();
            if (Delay == 0) ColorsDone = true;  // Setup completed
            else chThdSleepMilliseconds(Delay);
        }
    } // while true
}

void InitMirilli() {
    LumeLeds.Init(MIRILLI_LED_CNT);
    chThdCreateStatic(waMirilli, sizeof(waMirilli), NORMALPRIO, (tfunc_t)MirilliThread, NULL);
}

void SetTargetClrH(uint32_t H, Color_t Clr) {
    if(H >= MIRILLI_H_CNT) return;
    uint32_t Indx = H2LedN[H];
    ITargetClr[Indx] = Clr;
    // Special case
#ifdef SECOND_0_LED_INDX
    if(H == 0) {
        ITargetClr[SECOND_0_LED_INDX] = Clr;
    }
#endif
}
void SetTargetClrM(uint32_t M, Color_t Clr) {
    if(M >= MIRILLI_M_CNT) return;
    uint32_t Indx = M2LedN[M];
    ITargetClr[Indx] = Clr;
}

void WakeMirilli() {
    ColorsDone = false;
    if(PThd == nullptr) return;
    chSysLock();
    chThdResumeS(&PThd, MSG_OK);
    chSysUnlock();
}

void ResetColorsToOffState(Color_t ClrH, Color_t ClrM) {
    Color_t TargetClrH;
    Color_t TargetClrM;
#if MIRILLI_USE_SK6812
    TargetClrH.SetRGBWBrightness(ClrH, OFF_LUMINOCITY, 255);
    TargetClrM.SetRGBWBrightness(ClrM, OFF_LUMINOCITY, 255);
#else
    TargetClrH.SetRGBBrightness(ClrH, OFF_LUMINOCITY, 255);
    TargetClrM.SetRGBBrightness(ClrM, OFF_LUMINOCITY, 255);
#endif
    for(int32_t i=0; i<12; i++) {
        SetTargetClrH(i, TargetClrH);
        SetTargetClrM(i, TargetClrM);
    }
}
