#include "FlasksSnd.h"
#include "audiomixer.h"
#include "CS42L52.h"
#include "MsgQ.h"
#include "kl_fs_utils.h"

CS42L52_t Codec;
FlasksSnd_t Sound;

#define PRINT_FUNC()    Printf("snd: %S\r", __FUNCTION__)

std::string FNameAlive = "alive.wav";
std::string FNameAdd = "ding.wav";
std::string FNameRemove = "puff.wav";
std::string FNameBackground = "Background.wav";

#if 1 // ============================ SndCmd ===================================
enum SndCmd_t : uint8_t {
    sndcmdNone, sndcmdStart, sndCmdVolume,
    sndcmdStop, sndcmdPrepareNextBuf
};

union SndMsg_t {
    uint32_t DWord[3];
    struct {
        SndCmd_t Cmd;
        union {
            struct {
                uint8_t Slot;
                uint16_t Volume;
                std::string *PFName;
                bool Repeat;
                uint8_t EvtID;
            } __packed;
            struct {
                uint16_t VHum, VSwingL, VSwingH;
            } VolTriple __packed;
        };
    } __packed;
    SndMsg_t& operator = (const SndMsg_t &Right) {
        DWord[0] = Right.DWord[0];
        DWord[1] = Right.DWord[1];
        DWord[2] = Right.DWord[2];
        return *this;
    }
    SndMsg_t() : Cmd(sndcmdNone) {}
    SndMsg_t(SndCmd_t ACmd) : Cmd(ACmd) {}
    SndMsg_t(SndCmd_t ACmd, uint8_t ASlot) : Cmd(ACmd), Slot(ASlot) {}
    SndMsg_t(SndCmd_t ACmd, uint8_t ASlot, uint16_t AVolume) : Cmd(ACmd), Slot(ASlot), Volume(AVolume) {}
    SndMsg_t(SndCmd_t ACmd, uint8_t ASlot, std::string* AFName, bool ARepeat, uint8_t AEvtID = 0) :
        Cmd(ACmd), Slot(ASlot), PFName(AFName), Repeat(ARepeat), EvtID(AEvtID) {}
    SndMsg_t(SndCmd_t ACmd, uint16_t AVHum, uint16_t AVSwingL, uint16_t AVSwingH) {
        Cmd = ACmd;
        VolTriple.VHum = AVHum;
        VolTriple.VSwingL = AVSwingL;
        VolTriple.VSwingH = AVSwingH;
    }
} __packed;

static EvtMsgQ_t<SndMsg_t, 18> MsgQSnd;
#endif

#if 1 // =========================== Callbacks =================================
// Callbacks. Returns true if OK
size_t TellCallback(void *file_context) {
    FIL *pFile = (FIL*)file_context;
    return pFile->fptr;
}

bool SeekCallback(void *file_context, size_t offset) {
    FIL *pFile = (FIL*)file_context;
    FRESULT rslt = f_lseek(pFile, offset);
    if(rslt == FR_OK) return true;
    else {
        Printf("SeekErr %u\r", rslt);
        return false;
    }
}

size_t ReadCallback(void *file_context, uint8_t *buffer, size_t length) {
//    systime_t Start = chVTGetSystemTimeX();
    FIL *pFile = (FIL*)file_context;
    uint32_t ReadSz=0;
    FRESULT rslt = f_read(pFile, buffer, length, &ReadSz);
//    Printf("   r %u\r", chVTTimeElapsedSinceX(Start));
    if(rslt == FR_OK) return ReadSz;
    else {
//        Printf("ReadErr %u\r", rslt);
        return 0;
    }
}

void TrackEndCallback(int SlotN);
#endif

#if 1 // ====================== SlotPlayer and Mixer ===========================
static AudioMixer mixer {TellCallback, SeekCallback, ReadCallback, TrackEndCallback, 2};

// ==== Buffers ====
#define BUF_SZ_FRAME       1024UL
struct SndBuf_t {
    uint32_t Buf[BUF_SZ_FRAME], Sz;
};
static SndBuf_t Buf1, Buf2;
static volatile SndBuf_t *PCurBuf = &Buf1;

static void ReadToBuf(volatile SndBuf_t *PBuf) {
    PBuf->Sz = mixer.play((int16_t*)PBuf->Buf, BUF_SZ_FRAME); // returns how many frames was read
}

void TransmitBuf(volatile SndBuf_t *PBuf) {
    Codec.TransmitBuf(PBuf->Buf, PBuf->Sz*2);    // Sz16 == SzFrame*2
}

// DMA Tx Completed IRQ
extern "C"
void DmaSAITxIrq(void *p, uint32_t flags) {
    chSysLockFromISR();
    PCurBuf = (PCurBuf == &Buf1)? &Buf2 : &Buf1;
    TransmitBuf(PCurBuf);
    MsgQSnd.SendNowOrExitI(SndMsg_t(sndcmdPrepareNextBuf));
    chSysUnlockFromISR();
}
#endif

#if 1 // ============================ Slot =====================================
class Slot_t {
private:
    const uint32_t Indx;
    FIL IFile;
    uint8_t EvtID = 0;
    void SendEndEvtIfNeeded() {
        if(EvtID != 0) EvtQMain.SendNowOrExit(EvtMsg_t(EvtID));
    }
public:
    Slot_t(uint32_t AIndx) : Indx(AIndx), Busy(false), Volume(AudioMixer::UNIT_LEVEL) {}
    bool Busy;
    uint16_t Volume;

    void Start(std::string* AFname, bool ARepeat, uint8_t AEvtID) {
//        Printf("%S: %S %u %u\r", __FUNCTION__, AFname->c_str(), Volume, ARepeat);
        EvtID = AEvtID;
        if(TryOpenFileRead(AFname->c_str(), &IFile) == retvOk) {
            uint32_t SamplingRate = mixer.samplingRate(); // Save current Fs
            Busy = mixer.start(
                    Indx, &IFile,
                    (ARepeat? AudioMixer::Mode::Continuous : AudioMixer::Mode::Single),
                    true, Volume,
                    AudioTrack::Fade::LinearIn, START_STOP_FADE_DUR
            );
            // Check if Fs changed
            if(mixer.samplingRate() != SamplingRate) {
                Codec.SetupSampleRate(mixer.samplingRate());
//                Printf("New Fs: %u\r", mixer.samplingRate());
            }
        }
        else {
            SendEndEvtIfNeeded(); // openfile failed
        }
    }
    void SetVolume(uint16_t AVolume) {
        mixer.fade(Indx, AVolume);
        Volume = AVolume;
    }
    void SetVolume(uint16_t AVolume, uint16_t FadeDuration_ms) {
        mixer.fade(Indx, AVolume,
                (AVolume > Volume)? AudioTrack::Fade::LinearIn : AudioTrack::Fade::LinearOut,
                        FadeDuration_ms);
        Volume = AVolume;
    }
    void Stop() {
        if(Busy) {
            mixer.stop(Indx, AudioTrack::Fade::LinearOut, START_STOP_FADE_DUR);
//            CloseFile(&IFile); XXX
            Busy = false;
            SendEndEvtIfNeeded();
        }
    }
};
static Slot_t Slot[AudioMixer::TRACKS] = {0, 1, 2, 3, 4};
#endif

#if 1 // ============================== Thread =================================
static THD_WORKING_AREA(waSndThread, 4096);
__noreturn
static void SoundThread(void *arg) {
    chRegSetThreadName("Sound");
    // Fill two buffers
    ReadToBuf(&Buf1);
    ReadToBuf(&Buf2);
    PCurBuf = &Buf1;
    // Start playing
    TransmitBuf(&Buf1);

    while(true) {
        SndMsg_t Msg = MsgQSnd.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case sndcmdStart:
//                Printf("sndCmdStart %u %S\r", Msg.Slot, Msg.PFName->c_str());
                Slot[Msg.Slot].Start(Msg.PFName, Msg.Repeat, Msg.EvtID);
                break;

            case sndCmdVolume:
//                Log.Write("*setvol sl=%u v=%u\r", Msg.Slot, Msg.Volume);
//                Printf("*setvol sl=%u v=%u\r", Msg.Slot, Msg.Volume);
                if(Msg.Slot == ALL_SLOTS) mixer.scale(Msg.Volume);
                else Slot[Msg.Slot].SetVolume(Msg.Volume, FADE_DURATION_ms);
                break;

            case sndcmdStop:
//                Printf("sndCmdStop %u\r", Msg.Slot);
                if(Msg.Slot == ALL_SLOTS) {
                    for(uint32_t i=0; i<mixer.TRACKS; i++) Slot[i].Stop();
                }
                else Slot[Msg.Slot].Stop();
                break;

            case sndcmdPrepareNextBuf: {
//                systime_t Start = chVTGetSystemTimeX();
                ReadToBuf((PCurBuf == &Buf1)? &Buf2 : &Buf1);
//                Printf(" T %u\r", chVTTimeElapsedSinceX(Start));
            } break;

            case sndcmdNone: break;
        } // switch
    } // while true
}

// Mixer calls this
void TrackEndCallback(int SlotN) {
    Slot[SlotN].Stop();
    Sound.OnTrackEnd(SlotN);
//    Printf("TrackEnd: %u\r", SlotN);
}
#endif

#if 1 // ============================== SoundDir ===============================
uint8_t SndDir_t::Init() {
    DIR *PDir = new DIR;
    FILINFO *PInfo = new FILINFO;
    FNames.clear();
    PrevN = -1; // No prev file
    FRESULT Rslt = f_opendir(PDir, FullFName.c_str());
    if(Rslt == FR_OK) {
        while(true) { // Iterate items in dir
            *PInfo->fname = 0;
            Rslt = f_readdir(PDir, PInfo);
            if(Rslt != FR_OK) break;
            if(PInfo->fname[0] == 0) break; // No files left
            else { // Filename ok, check if not dir
                if(!(PInfo->fattrib & AM_DIR)) { // Not dir
                    // Check Ext
                    char *FName = PInfo->fname;
                    uint32_t Len = strlen(FName);
                    if(Len > 4) {
                        if(strncasecmp(&FName[Len-3], "wav", 3) == 0) {
                            FNames.push_back(FName);
//                            Printf("  %S %S\r", DirName, FName);
                        }
                    } // if Len>4
                } // if not dir
            } // Filename ok
        } // while true
    } // if f_opendir
    delete PDir;
    delete PInfo;
    return (FNames.size() == 0)? retvEmpty : retvOk;
}

uint8_t SndDir_t::SelectRandomFilename() {
    size_t Cnt = FNames.size();
    if(Cnt > 0) {
        // Generate number of file
        int32_t N = 0;
        if(Cnt > 1) { // Get random number if count > 1
            do {
                N = Random::Generate(0, Cnt-1); // [0; Cnt-1]
            } while(N == PrevN);   // skip same as previous
        }
        PrevN = N;
        FullFName  = DirName + '/' + FNames[N];
        return retvOk;
    }
    else return retvEmpty; // No files
}
#endif

#if 1 // ============================ Sound ====================================
void FlasksSnd_t::Init() {
    // Audio codec
    Codec.Init();   // i2c initialized inside, as pull-ups powered by VAA's LDO
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Stereo);  // Always
    Codec.SetupSampleRate(22050); // Just default, will be replaced when changed
    // Mixer
    MsgQSnd.Init();
    chThdCreateStatic(waSndThread, sizeof(waSndThread), NORMALPRIO, SoundThread, NULL);
}

// Volume: [0; 100]
void FlasksSnd_t::SetupVolume(int32_t Volume) {
    Printf("Volume: %d\r", Volume);
    // Set software volume to maximum
    MsgQSnd.SendNowOrExit(SndMsg_t(sndCmdVolume, ALL_SLOTS, 4096));
    // Setup hardware volume to what needed
    Codec.SetMasterVolume(12); // max
    Codec.SetSpeakerVolume(0); // max
    if(Volume > 0) Codec.SetMasterVolume(Volume-88);  // [0...100] => [-88...+12]
    else Codec.SetMasterVolume(-99);
}

void FlasksSnd_t::SetSlotVolume(uint8_t ASlot, int32_t Volume) {
    MsgQSnd.SendNowOrExit(SndMsg_t(sndCmdVolume, ASlot, Volume));
}

bool FlasksSnd_t::IsIdle() {
    for(uint32_t i=1; i<mixer.TRACKS; i++) {
        if(Slot[i].Busy) return false;
    }
    return true;
}

void FlasksSnd_t::StopAll() {
    MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStop, ALL_SLOTS));
}

void FlasksSnd_t::IPlayDir(SndDir_t &ADir, uint8_t ASlotN, bool Repeat, uint8_t EvtId) {
    if(ADir.SelectRandomFilename() == retvOk) {
        MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, ASlotN, &ADir.FullFName, Repeat, EvtId));
    }
    else { // No file, play ended not being started
        if(EvtId != evtIdNone) EvtQMain.SendNowOrExit(EvtMsg_t(EvtId));
    }
}

void FlasksSnd_t::PlayAlive() {
    MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, 0, &FNameAlive, DO_NOT_REPEAT, evtIdNone));
}

void FlasksSnd_t::PlayAdd(uint8_t ASlot) {
    MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, ASlot, &FNameAdd, DO_NOT_REPEAT, evtIdNone));
}

void FlasksSnd_t::PlayRemove(uint8_t ASlot) {
    MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, ASlot, &FNameRemove, DO_NOT_REPEAT, evtIdNone));
}

void FlasksSnd_t::PlayBackgroundIfNotYet() {
    if(!Slot[BACKGROUND_SLOT].Busy) {
        MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, BACKGROUND_SLOT, &FNameBackground, REPEAT, evtIdNone));
    }
}

void FlasksSnd_t::StopBackground() {
    MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStop, BACKGROUND_SLOT));
}

void FlasksSnd_t::OnTrackEnd(int SlotN) {
//    Printf("%S %u\r", __FUNCTION__, SlotN);
    // Play either next file or Hum
//        Printf("Now1: %u; Next: %u\r", NowFName.length(), NextFName.length());
//    if(NextFName.length() > 0) {
//        NowFName = NextFName;
//        NextFName.clear(); // Invalidate it to play hum after
////            Printf("Now2: %u; Next: %u\r", NowFName.length(), NextFName.length());
//        MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, SLOT_SETTINGS, &NowFName, DO_NOT_REPEAT));
//    }
//    else if(ThdRef != nullptr) chThdResume(&ThdRef, MSG_OK); // Wake waiting thread if any
//    else { // play hum
//        MsgQSnd.SendNowOrExit(SndMsg_t(sndcmdStart, SLOT_SETTINGS, &HumFName, REPEAT));
//    }
}
#endif
