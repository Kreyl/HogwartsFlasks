/*
 * Sequences.h
 *
 *  Created on: 09 ���. 2015 �.
 *      Author: Kreyl
 */

#ifndef SEQUENCES_H__
#define SEQUENCES_H__

#include "ChunkTypes.h"

#if 1 // ============================ LED blink ================================
const BaseChunk_t lsqCmd[] = {
        {csSetup, 0},
        {csWait, 99},
        {csSetup, 1},
        {csEnd}
};

const BaseChunk_t lsqBlink[] = {
        {csSetup, 1},
        {csWait, 99},
        {csSetup, 0},
        {csEnd}
};

const BaseChunk_t lsqChecking[] = {
        {csSetup, 1},
        {csWait, 36},
        {csSetup, 0},
        {csWait, 36},
        {csGoto, 0}
};

const BaseChunk_t lsqIdle[] = {
        {csSetup, 0},
        {csWait, 99},
        {csSetup, 1},
        {csEnd, 0}
};
#endif

#if 0 // ============================ LED RGB ==================================
const LedRGBChunk_t lsqStart[] = {
        {csSetup, 0, clDarkRed},
        {csWait, 180},
        {csSetup, 0, clDarkGreen},
        {csWait, 180},
        {csSetup, 0, clDarkBlue},
        {csWait, 180},
        {csSetup, 0, clBlack},
        {csEnd}
};

const LedRGBChunk_t lsqFailure[] = {
        {csSetup, 0, clRed},
        {csWait, 99},
        {csSetup, 0, clBlack},
        {csWait, 99},
        {csSetup, 0, clRed},
        {csWait, 99},
        {csSetup, 0, clBlack},
        {csWait, 99},
        {csSetup, 0, clRed},
        {csWait, 99},
        {csSetup, 0, clBlack},
        {csEnd}
};

const LedRGBChunk_t lsqDischarged[] = {
        {csSetup, 0, clRed},
        {csWait, 270},
        {csSetup, 0, clBlack},
        {csWait, 720},
        {csGoto, 0}
};

const LedRGBChunk_t lsqCharging[] = {
        {csSetup, 360, clGreen},
        {csSetup, 360, clBlack},
        {csWait, 630},
        {csGoto, 0}
};

const LedRGBChunk_t lsqChargingDone[] = {
        {csSetup, 0, clGreen},
        {csEnd}
};

#endif

#if 0 // =========================== LED Smooth ================================
#define LED_TOP_BRIGHTNESS  100

const LedSmoothChunk_t lsqFadeIn[] = {
        {csSetup, 360, LED_TOP_BRIGHTNESS},
        {csEnd}
};
const LedSmoothChunk_t lsqFadeOut[] = {
        {csSetup, 630, 0},
        {csEnd}
};
const LedSmoothChunk_t lsqStart[] = {
        {csSetup, 360, LED_TOP_BRIGHTNESS},
        {csSetup, 360, 0},
        {csEnd}
};
const LedSmoothChunk_t lsqEnterIdle[] = {
        {csSetup, 360, 0},
        {csEnd}
};

#endif

#if 1 // ============================= Beeper ==================================
#define BEEP_VOLUME     1   // Maximum 10

#if 1 // ==== Notes ====
#define La_2    880

#define Do_3    1047
#define Do_D_3  1109
#define Re_3    1175
#define Re_D_3  1245
#define Mi_3    1319
#define Fa_3    1397
#define Fa_D_3  1480
#define Sol_3   1568
#define Sol_D_3 1661
#define La_3    1720
#define Si_B_3  1865
#define Si_3    1976

#define Do_4    2093
#define Do_D_4  2217
#define Re_4    2349
#define Re_D_4  2489
#define Mi_4    2637
#define Fa_4    2794
#define Fa_D_4  2960
#define Sol_4   3136
#define Sol_D_4 3332
#define La_4    3440
#define Si_B_4  3729
#define Si_4    3951

// Length
#define OneSixteenth    90
#define OneEighth       (OneSixteenth * 2)
#define OneFourth       (OneSixteenth * 4)
#define OneHalfth       (OneSixteenth * 8)
#define OneWhole        (OneSixteenth * 16)
#endif

// MORSE
#define MORSE_TONE {csSetup, BEEP_VOLUME, Do_3}
#define MORSE_DOT_LENGTH 180
#define MORSE_DASH_LENGTH MORSE_DOT_LENGTH * 3
#define MORSE_PAUSE_LENGTH MORSE_DOT_LENGTH
#define MORSE_PAUSE {csSetup, 0}, {csWait, MORSE_PAUSE_LENGTH}
#define MORSE_DOT MORSE_TONE, {csWait, MORSE_DOT_LENGTH}, MORSE_PAUSE
#define MORSE_DASH MORSE_TONE, {csWait, MORSE_DASH_LENGTH}, MORSE_PAUSE

// Type, BEEP_VOLUME, freq
const BeepChunk_t bsqOn[] = {
        {csSetup, 10, 7000},
        {csEnd}
};

const BeepChunk_t bsqButton[] = {
        {csSetup, 1, 1975},
        {csWait, 54},
        {csSetup, 0},
        {csEnd}
};
const BeepChunk_t bsqBeepBeep[] = {
        {csSetup, BEEP_VOLUME, 1975},
        {csWait, 63},
        {csSetup, 0},
        {csWait, 63},
        {csSetup, BEEP_VOLUME, 1975},
        {csWait, 63},
        {csSetup, 0},
        {csEnd}
};

const BeepChunk_t bsqCharge[] = {
        MORSE_DOT, MORSE_DASH,
        {csEnd}
};
const BeepChunk_t bsqThrow[] = {
        MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqPunch[] = {
        MORSE_DOT, MORSE_DASH, MORSE_DASH, MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqLift[] = {
        MORSE_DOT, MORSE_DASH, MORSE_DOT, MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqWarp[] = {
        MORSE_DOT, MORSE_DASH, MORSE_DASH,
        {csEnd}
};
const BeepChunk_t bsqBarrier[] = {
        MORSE_DASH, MORSE_DOT, MORSE_DOT, MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqCleanse[] = {
        MORSE_DASH, MORSE_DOT, MORSE_DASH, MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqSingular[] = {
        MORSE_DOT, MORSE_DOT, MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqSong[] = {
        {csSetup, BEEP_VOLUME, Sol_3},
        {csWait, 360},
        {csSetup, BEEP_VOLUME, Mi_3},
        {csWait, 360},
        {csSetup, BEEP_VOLUME, Do_3},
        {csWait, 360},
        {csSetup, 0},
        {csEnd}
};
const BeepChunk_t bsqRelease[] = {
        MORSE_DOT, MORSE_DASH, MORSE_DOT,
        {csEnd}
};
const BeepChunk_t bsqPwrRelease[] = {
        MORSE_DASH, MORSE_DOT, MORSE_DOT, MORSE_DASH,
        {csEnd}
};


#if 1 // ==== Extensions ====
// Pill
const BeepChunk_t bsqBeepPillOk[] = {
        {csSetup, BEEP_VOLUME, Si_3},
        {csWait, 180},
        {csSetup, BEEP_VOLUME, Re_D_4},
        {csWait, 180},
        {csSetup, BEEP_VOLUME, Fa_D_4},
        {csWait, 180},
        {csSetup, 0},
        {csEnd}
};

const BeepChunk_t bsqBeepPillBad[] = {
        {csSetup, BEEP_VOLUME, Fa_4},
        {csWait, 180},
        {csSetup, BEEP_VOLUME, Re_4},
        {csWait, 180},
        {csSetup, BEEP_VOLUME, Si_3},
        {csWait, 180},
        {csSetup, 0},
        {csEnd}
};
#endif // ext
#endif // beeper

#if 1 // ============================== Vibro ==================================
#define VIBRO_VOLUME        100

#define VIBRO_SHORT_MS      99

const BaseChunk_t vsqBrrBrr[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 99},
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csEnd}
};

const BaseChunk_t vsqActive[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, 99},
        {csSetup, 0},
        {csWait, 720},
        {csGoto, 0}
};


// Gestures
#define VMORSE_TONE         {csSetup, VIBRO_VOLUME}
#define VMORSE_DOT_LENGTH   180
#define VMORSE_DASH_LENGTH  VMORSE_DOT_LENGTH * 3
#define VMORSE_PAUSE_LENGTH VMORSE_DOT_LENGTH
#define VMORSE_PAUSE        {csSetup, 0}, {csWait, VMORSE_PAUSE_LENGTH}
#define VMORSE_DOT          VMORSE_TONE, {csWait, VMORSE_DOT_LENGTH}, VMORSE_PAUSE
#define VMORSE_DASH         VMORSE_TONE, {csWait, VMORSE_DASH_LENGTH}, VMORSE_PAUSE


const BaseChunk_t vsqCharge[] = {
        VMORSE_DOT, VMORSE_DASH,
        {csEnd}
};
const BaseChunk_t vsqThrow[] = {
        VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqPunch[] = {
        VMORSE_DOT, VMORSE_DASH, VMORSE_DASH, VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqLift[] = {
        VMORSE_DOT, VMORSE_DASH, VMORSE_DOT, VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqWarp[] = {
        VMORSE_DOT, VMORSE_DASH, VMORSE_DASH,
        {csEnd}
};
const BaseChunk_t vsqBarrier[] = {
        VMORSE_DASH, VMORSE_DOT, VMORSE_DOT, VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqCleanse[] = {
        VMORSE_DASH, VMORSE_DOT, VMORSE_DASH, VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqSingular[] = {
        VMORSE_DOT, VMORSE_DOT, VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqSong[] = {
        {csSetup, 30},
        {csWait, 360},
        {csSetup, 60},
        {csWait, 360},
        {csSetup, 100},
        {csWait, 360},
        {csSetup, 0},
        {csEnd}
};
const BaseChunk_t vsqRelease[] = {
        VMORSE_DOT, VMORSE_DASH, VMORSE_DOT,
        {csEnd}
};
const BaseChunk_t vsqPwrRelease[] = {
        VMORSE_DASH, VMORSE_DOT, VMORSE_DOT, VMORSE_DASH,
        {csEnd}
};




/*
const BaseChunk_t vsqError[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, 999},
        {csSetup, 0},
        {csEnd}
};

const BaseChunk_t vsqSingle[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 1800},
        {csGoto, 0}
};
const BaseChunk_t vsqPair[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 99},
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 1350},
        {csGoto, 0}
};
const BaseChunk_t vsqMany[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 99},
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 99},
        {csSetup, VIBRO_VOLUME},
        {csWait, VIBRO_SHORT_MS},
        {csSetup, 0},
        {csWait, 1008},
        {csGoto, 0}
};
*/
#endif

#endif //SEQUENCES_H__