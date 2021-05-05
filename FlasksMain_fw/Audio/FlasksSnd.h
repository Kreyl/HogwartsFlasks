#pragma once

#include <inttypes.h>
#include <deque>
#include <string>
#include "ch.h"
#include "EvtMsgIDs.h"

#if 1 // ========= Constants ==========
#define REPEAT              true
#define DO_NOT_REPEAT       false

// Slots
#define ALL_SLOTS           0xFF
#define BACKGROUND_SLOT     4

#define FADE_DURATION_ms    450
#define START_STOP_FADE_DUR 450
#endif

class SndDir_t {
private:
    int32_t PrevN;
    const char* DirName;
    std::deque<std::string> FNames;
public:
    std::string FullFName;
    SndDir_t(const char* AName) : PrevN(0), DirName(AName) {}
    uint8_t Init();
    uint8_t SelectRandomFilename();
};

class FlasksSnd_t {
private:
    thread_reference_t ThdRef;
    std::string NowFName;
    std::string NextFName;
    // Subdirs
    SndDir_t DirAdd{"Add"};
//    SndDir_t DirRemove{"Remove"};
//    SndDir_t DirFlyD{"FlyDown"};
//    SndDir_t DirBlaster{"FlyUp"};

    void IPlayDir(SndDir_t &ADir, uint8_t ASlotN, bool Repeat, uint8_t EvtId);
public:
    void Init();
    void SetupVolume(int32_t Volume);
    void SetSlotVolume(uint8_t ASlot, int32_t Volume);
    bool IsIdle();
    void StopAll();
    // ==== Sound effects ====
    void PlayAlive();
    void PlayAdd(uint8_t ASlot);
    void PlayRemove(uint8_t ASlot);
    void PlayBackgroundIfNotYet();
    void StopBackground();
    void OnTrackEnd(int SlotN);
};

extern FlasksSnd_t Sound;
