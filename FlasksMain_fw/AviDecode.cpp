/*
 * AviDecode.cpp
 *
 *  Created on: 8 авг. 2020 г.
 *      Author: layst
 */

#include "AviDecode.h"
#include "kl_fs_utils.h"
#include "jpeg_utils.h"
#include "kl_jpeg.h"
#include "MsgQ.h"
#include "lcdtft.h"
#include "CS42L52.h"
#include "kl_buf.h"

//__attribute__ ((section ("DATA_RAM")))

static FIL ifile;
extern CS42L52_t Codec;

#if 1 // ============================ AVI headers ==============================
#define IN_AUBUF_SZ     0x2000UL // Packed frame must fit here
#define IN_AUBUF_SZ32   (IN_VBUF_SZ / 4)

#define FOURCC(c1, c2, c3, c4)  ((uint32_t)((c4 << 24) | (c3 << 16) | (c2 << 8) | c1))

class ChunkHdr_t {
public:
    uint32_t ckID;
    uint32_t ckSize = 0; //  includes the size of listType plus the size of listData
    uint32_t Type;
    uint8_t Read() {
        if(f_eof(&ifile)) return retvEndOfFile;
        uint32_t BytesRead = 0;
        if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
        DataStart = f_tell(&ifile);
        return retvOk;
    }

    uint8_t ReadNext() {
        if(DataStart != 0) {
            if(ckSize & 1) ckSize++; // The data is always padded to nearest WORD boundary. ckSize gives the size of the valid data in the chunk; it does not include the padding
            f_lseek(&ifile, DataStart + ckSize);
        }
        return Read();
    }
    uint8_t ReadType() {
        uint32_t BytesRead = 0;
        if(f_read(&ifile, &Type, 4, &BytesRead) != FR_OK or BytesRead != 4) return retvFail;
        ckSize -= BytesRead;
        DataStart += BytesRead;
        return retvOk;
    }

    void Print() { Printf("%4S; Sz=%u\r", &ckID, ckSize); }
    void PrintWType() { Printf("%4S; Sz=%u; %4S\r", &ckID, ckSize, &Type); }
    bool IsRiff() { return ckID == FOURCC('R', 'I', 'F', 'F'); }
    bool IsList() { return ckID == FOURCC('L', 'I', 'S', 'T'); }
    bool IsJUNK() { return ckID == FOURCC('J', 'U', 'N', 'K'); }
    bool IsVideo() { return ckID == FOURCC('0','0','d','c'); }
    bool IsAudio() { return ckID == FOURCC('0','1','w','b'); }
    bool TypeIsHDRL() { return Type == FOURCC('h', 'd', 'r', 'l'); }
    bool TypeIsMOVI() { return Type == FOURCC('m', 'o', 'v', 'i'); }
    bool TypeIsAVI()  { return Type == FOURCC('A', 'V', 'I', ' '); }
    bool TypeIsidx1() { return Type == FOURCC('i', 'd', 'x', '1'); }
private:
    uint32_t DataStart = 0;
};

class Chunk_t {
public:
    uint32_t ckID;
    uint32_t ckSize = 0; //  includes the size of listType plus the size of listData
    void MoveTo(uint32_t NewLoc) { NextLoc = NewLoc; }
    uint8_t ReadHdr() {
        if(f_tell(&ifile) != NextLoc) f_lseek(&ifile, NextLoc);
        if(f_eof(&ifile)) return retvEndOfFile;
        uint32_t BytesRead = 0;
        if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
        if(ckSize & 1) ckSize++; // The data is always padded to nearest WORD boundary. ckSize gives the size of the valid data in the chunk; it does not include the padding
        NextLoc = f_tell(&ifile) + ckSize;
        return retvOk;
    }

    uint8_t ReadData(uint32_t* Buf, uint32_t *ReadSz) {
        if(f_read(&ifile, Buf, ckSize, ReadSz) == FR_OK) return retvOk;
        else return retvFail;
    }

    void Print() { Printf("%4S; Sz=%u\r", &ckID, ckSize); }
    bool IsVideo() { return ckID == FOURCC('0','0','d','c'); }
    bool IsAudio() { return ckID == FOURCC('0','1','w','b'); }
private:
    uint32_t NextLoc = 0;
};




struct avimainheader_t {
    uint32_t fcc;
    DWORD  cb;
    DWORD  dwMicroSecPerFrame;
    DWORD  dwMaxBytesPerSec;
    DWORD  dwPaddingGranularity;
    DWORD  dwFlags;
    DWORD  dwTotalFrames;
    DWORD  dwInitialFrames;
    DWORD  dwStreams;
    DWORD  dwSuggestedBufferSize;
    DWORD  dwWidth;
    DWORD  dwHeight;
    DWORD  dwReserved[4];
    uint8_t Read() {
        if(f_eof(&ifile)) return retvEndOfFile;
        uint32_t BytesRead = 0;
        if(f_read(&ifile, this, sizeof(avimainheader_t), &BytesRead) != FR_OK or BytesRead != sizeof(avimainheader_t)) return retvFail;
        if(fcc != FOURCC('a', 'v', 'i', 'h')) { Printf("BadAviHdr\r"); return retvFail; }
        return retvOk;
    }
    void Print() { Printf(" sz: %u\r usPerF: %u\r MaxBPS: %u\r Padding: %u\r ToalFrames: %u\r Streams: %u\r BufSz: %u\r Width: %u\r Height: %u\r",
            cb, dwMicroSecPerFrame, dwMaxBytesPerSec, dwPaddingGranularity, dwTotalFrames, dwStreams, dwSuggestedBufferSize, dwWidth, dwHeight); }
};

struct avistreamheader_t {
    uint32_t fcc;
    DWORD cb;
    uint32_t fccType;
    uint32_t fccHandler;
    DWORD dwFlags;
    WORD wPriority;
    WORD wLanguage;
    DWORD dwInitialFrames;
    DWORD dwScale;
    DWORD dwRate;
    DWORD dwStart;
    DWORD dwLength;
    DWORD dwSuggestedBufferSize;
    DWORD dwQuality;
    DWORD dwSampleSize;
    struct {
        short int left;
        short int top;
        short int right;
        short int bottom;
    } rcFrame;
    uint8_t Read() {
        if(f_eof(&ifile)) return retvEndOfFile;
        uint32_t BytesRead = 0;
        if(f_read(&ifile, this, sizeof(avistreamheader_t), &BytesRead) != FR_OK or BytesRead != sizeof(avistreamheader_t)) return retvFail;
        if(fcc != FOURCC('s', 't', 'r', 'h')) { Printf("BadStreamHdr %4S\r", &fcc); return retvFail; }
        return retvOk;
    }
    void Print() {
        Printf(" sz: %u\r Type: %4S\r Handler: %4S\r dwInitialFrames: %u\r ScaleRate: %u %u\r",
            cb, &fccType, &fccHandler, dwInitialFrames, dwScale, dwRate);
    }
};

struct WAVEFORMATEX {
  WORD  wFormatTag;
  WORD  nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD  nBlockAlign;
  WORD  wBitsPerSample;
  WORD  cbSize;
  uint8_t Read() {
      if(f_eof(&ifile)) return retvEndOfFile;
      uint32_t BytesRead = 0;
      if(f_read(&ifile, this, sizeof(WAVEFORMATEX), &BytesRead) != FR_OK or BytesRead != sizeof(WAVEFORMATEX)) return retvFail;
      return retvOk;
  }
  void Print() {
      Printf("  Tag: %u\r  nCh: %u\r  nSmpPS: %u\r  Align: %u\r  nBPS: %u\r  Sz: %u\r", wFormatTag, nChannels, nSamplesPerSec, nBlockAlign, wBitsPerSample, cbSize);
  }
};

#define AUBUF_SZ    8192
#define AUBUF_CNT   (AUBUF_SZ / 4)

class {
private:
    uint32_t Buf1[AUBUF_CNT];
    uint32_t Buf2[AUBUF_CNT];
public:
//    uint32_t *Buf1, *Buf2;

    uint32_t *CurrBuf= Buf1, Sz = 0;

    void Switch() {
        if(CurrBuf == Buf1) {
            CurrBuf = Buf2;
        }
        else {
            CurrBuf = Buf1;
            CurrBuf = Buf2;
        }
    }
} AuBuf __attribute__ ((section ("DATA_RAM")));


//#define BIGBUF_SZ    0x3D4000UL
//#define BIGBUF_CNT   (BIGBUF_SZ / 4)
//uint32_t *BigBuf;

enum FileCmd_t { fcmdNone, fcmdReadNextFrame, fcmdReadNextAudio };

union FileMsg_t {
    uint32_t DWord[2];
    struct {
        FileCmd_t Cmd;
        uint32_t *Ptr = nullptr;
    } __packed;
    FileMsg_t& operator = (const FileMsg_t &Right) {
        DWord[0] = Right.DWord[0];
        DWord[1] = Right.DWord[1];
        return *this;
    }
    FileMsg_t() : Cmd(fcmdNone) {}
    FileMsg_t(FileCmd_t ACmd) : Cmd(ACmd) {}
} __packed;

#define IN_VBUF_SZ      0xA000UL // Packed frame must fit here
#define IN_VBUF_SZ32    (IN_VBUF_SZ / 4)

static THD_WORKING_AREA(waFileThd, 1024);
static void FileThd(void *arg);

class VFileReader_t {
private:
    thread_t *ThdPtr;
    Chunk_t VCk;
    EvtMsgQ_t<FileMsg_t, 9> MsgQFile;
    uint32_t EndLoc = 0;
    uint32_t NextFrameLoc = 0;
    uint32_t VBuf1[IN_VBUF_SZ32], VBuf2[IN_VBUF_SZ32];
    // Audio
//    Buf_t AuBufs[2];

    uint8_t ReadNextFrame() {
        uint32_t *NextBuf = VBuf;
        VBuf = nullptr;
        systime_t start = chVTGetSystemTimeX();
        while(true) {
            if(VCk.ReadHdr() != retvOk) break;
            if(VCk.IsVideo()) { // found
                if(VCk.ckSize > IN_VBUF_SZ) {
                    Printf("Too large data\r");
                    return retvFail;
                }
                // Read data
                NextBuf = (NextBuf == VBuf1)? VBuf2 : VBuf1;
                if(VCk.ReadData(NextBuf, &VSz) != retvOk) break;
                sysinterval_t ela = chVTTimeElapsedSinceX(start);
                if(ela > 418) Printf("rne %u *********\r", ela);
//                else Printf("rne %u\r", ela);
                VBuf = NextBuf;
                return retvOk;
            }
        }
        return retvFail;
    }

    void ReadNextAudio() {

    }
public:
    uint32_t *VBuf = nullptr, VSz = 0;
    bool IsLastFrame = false;

    void Init() {
        MsgQFile.Init();
//        Buf1 = (uint32_t*)malloc(IN_VBUF_SZ);
//        Buf2 = (uint32_t*)malloc(IN_VBUF_SZ);

//        AuBuf.Buf1 = (uint32_t*)malloc(AUBUF_SZ);
//        AuBuf.Buf2 = (uint32_t*)malloc(AUBUF_SZ);
        chThdCreateStatic(waFileThd, sizeof(waFileThd), NORMALPRIO, (tfunc_t)FileThd, NULL);
    }

    void Start(uint32_t VideoSz) {
        EndLoc = f_tell(&ifile) + VideoSz;
        VCk.MoveTo(f_tell(&ifile));
        PrepareNextFrame();
    }

    void PrepareNextFrame() { MsgQFile.SendNowOrExit(FileMsg_t(fcmdReadNextFrame)); }
    void PrepareNextAudioI() { MsgQFile.SendNowOrExitI(FileMsg_t(fcmdReadNextAudio)); }

    __noreturn
    void ITask() {
        while(true) {
            FileMsg_t Msg = MsgQFile.Fetch(TIME_INFINITE);
            switch(Msg.Cmd) {
                case fcmdReadNextFrame:
                    IsLastFrame = (ReadNextFrame() != retvOk);
                    break;
                case fcmdReadNextAudio:
                    ReadNextAudio();
                    break;
                default: break;
            }
        } // while true
    }
} VFileReader;

__noreturn
static void FileThd(void *arg) {
    chRegSetThreadName("VideoFile");
    VFileReader.ITask();
}

////__attribute__ ((section ("DATA_RAM")))
//class AudioChunk_t {
//public:
//    uint32_t ckID = 0; // }
//    uint32_t Sz = 0;   // } Will be read together
//    uint32_t Buf[IN_AUBUF_SZ32];
//    uint32_t NextLoc = 0;
//
//    uint8_t ReadNext() {
//        f_lseek(&ifile, NextLoc); // Goto next chunk
//        while(true) {
//            uint32_t BytesRead = 0;
//            // Read next header
//            if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
////            Printf("%4S; Sz=%u\r", &ckID, Sz);
//            if(ckID == FOURCC('0','1','w','b')) { // found
//                if(Sz > IN_AUBUF_SZ) {
//                    Printf("Too large data\r");
//                    return retvFail;
//                }
//                // Read data
////                if(f_read(&ifile, Buf, Sz, &BytesRead) == FR_OK and BytesRead == Sz) {
//////                    cacheBufferFlush(Buf, Sz);
////                    // Prepare next loc
////                    NextLoc = f_tell(&ifile);
////                    if(NextLoc & 1) NextLoc++;
//                    return retvOk;
////                }
//            }
//            else {
//                if(Sz & 1) Sz++;
//                f_lseek(&ifile, f_tell(&ifile) + Sz);
//            }
//        }
//        return retvFail;
//    }
//} AudioChunk;

//#define FILEBUF_SZ  80000
//class VFileBuf_t {
//private:
////    uint8_t *Buf1, *Buf2, *WBuf;
//    uint8_t Buf1[FILEBUF_SZ];
//    uint32_t Sz1, Sz2;
//    uint8_t *pNextFrame;
//public:
//    uint8_t *RBuf;
//
//    uint8_t *FrameData;
//    uint32_t FrameSz;
//
//    void Init() {
////        Buf1 = (uint8_t*)malloc(FILEBUF_SZ);
////        Buf2 = (uint8_t*)malloc(FILEBUF_SZ);
//    }
//
//    uint8_t FillNextBuf() {
//        return retvOk;
////        uint32_t *WBuf = (RBuf == Buf2)?
//    }
//
//    uint8_t Start() {
//        Sz1 = 0;
//        Sz2 = 0;
//        RBuf = Buf1;
////        if(FillNextBuf() == retvFail) return retvFail;
////        if(FillNextBuf() == retvFail) return retvFail;
//        if(f_read(&ifile, Buf1, FILEBUF_SZ, &Sz1) != FR_OK) return retvFail;
////        if(f_read(&ifile, Buf2, FILEBUF_SZ, &Sz2) != FR_OK) return retvFail;
//        RBuf = Buf1;
//        pNextFrame = RBuf;
//        return retvOk;
//    }
//
//    uint8_t GetNextFrame() {
//        while(pNextFrame < (Buf1 + Sz1)) {
//            // Read next header
//            uint32_t ckID = *(uint32_t*)pNextFrame;
//            pNextFrame += 4;
//            FrameSz = *(uint32_t*)pNextFrame;
//            pNextFrame += 4;
//            Printf("%4S; Sz=%u\r", &ckID, FrameSz);
//            if(ckID == FOURCC('0','0','d','c')) { // found
//                FrameData = pNextFrame;
//                // Prepare next loc
//                pNextFrame += FrameSz;
//                if(FrameSz & 1) pNextFrame++;
//                return retvOk;
//            }
//            else {
//                pNextFrame += FrameSz;
//                if(FrameSz & 1) pNextFrame++;
//            }
//        }
//        return retvFail;
//    }
//} VFileBuf;


#endif

#if 1 // ============================ Video queue ==============================
enum VideoCmd_t { vcmdNone, vcmdStart, vcmdStop, vcmdNewMCUBufReady, vcmdEndDecode };

union VideoMsg_t {
    uint32_t DWord[2];
    struct {
        VideoCmd_t Cmd;
        uint32_t *Ptr = nullptr;
    } __packed;
    VideoMsg_t& operator = (const VideoMsg_t &Right) {
        DWord[0] = Right.DWord[0];
        DWord[1] = Right.DWord[1];
        return *this;
    }
    VideoMsg_t() : Cmd(vcmdNone) {}
    VideoMsg_t(VideoCmd_t ACmd) : Cmd(ACmd) {}
} __packed;

static EvtMsgQ_t<VideoMsg_t, 9> MsgQVideo;
#endif

#if 1 // =============================== DMA2D =================================
// When the DMA2D operates in memory-to-memory operation with pixel format conversion (no blending operation), the BG FIFO is not activated
namespace Dma2d {
void Init() {
    rccEnableDMA2D(FALSE);
    DMA2D->CR = (0b01UL << 16); // PFC
    DMA2D->AMTCR = (64UL << 8) | 1UL;
    DMA2D->FGOR = 0; // No offset
    // Alpha = 0xFF, no RedBlueSwap, no Alpha Inversion, replace Alpha, ARGB8888
    DMA2D->FGPFCCR = (0xFFUL << 24) | (0b01UL << 16);
    DMA2D->OPFCCR = 0; // No Red Blue swap, no alpha inversion, ARGB8888
}

void CopyBuffer(uint32_t *pSrc, uint32_t *pDst, uint32_t x, uint32_t y, uint32_t xsize, uint32_t ysize, uint32_t width_offset) {
//    Printf("S %X\r", pSrc);
    DMA2D->FGMAR = (uint32_t)pSrc; // Input data
    // Output
    DMA2D->OMAR = (uint32_t) pDst + (y * LCD_WIDTH + x) * 4;
    DMA2D->OOR = LCD_WIDTH - xsize; // Output offset
    DMA2D->NLR = (xsize << 16) | ysize; // Output PixelPerLine and Number of Lines
    // Start
    DMA2D->IFCR = 0x3FUL; // Clear all flags
    DMA2D->CR |= DMA2D_CR_START;
}

void WaitCompletion() { while(!(DMA2D->ISR & DMA2D_ISR_TCIF)); }
void Suspend() { DMA2D->CR |=  DMA2D_CR_SUSP; }
void Resume()  { DMA2D->CR &= ~DMA2D_CR_SUSP; }
}; // namespace
#endif

#if 1 // ============================= Decoding ================================
#define OUTBUF_SZ   (LCD_HEIGHT * LCD_WIDTH * 4UL) // 4 bytes per pixel
#define MCU_BUF_CNT ((384UL * 8UL) / sizeof(uint32_t)) // Multiple of 384

uint32_t MCU_BlockIndex = 0;
sysinterval_t FrameRate = 0;

class {
private:
    uint32_t Buf1[MCU_BUF_CNT];
    uint32_t Buf2[MCU_BUF_CNT];
    uint32_t *BufToFill = Buf1;
public:
    uint32_t *BufToProcess = Buf2, DataSzToProcess = 0;

    void PrepareToFill() { Jpeg::EnableOutDma(BufToFill, MCU_BUF_CNT); }

    void Switch() {
        Jpeg::DisableOutDma();
        DataSzToProcess = (MCU_BUF_CNT - Jpeg::GetOutDmaTransCnt()) * sizeof(uint32_t);
        if(BufToFill == Buf1) {
            BufToFill = Buf2;
            BufToProcess = Buf1;
        }
        else {
            BufToFill = Buf1;
            BufToProcess = Buf2;
        }
    }
} JMcuBuf;

void StartFrameDecoding() {
    Jpeg::Stop();
    MCU_BlockIndex = 0;
    Jpeg::PrepareToStart(VFileReader.VBuf, (VFileReader.VSz + 3) / 4);
    JMcuBuf.PrepareToFill();
    Jpeg::Start();
}

uint8_t ProcessMcuBuf() {
    Dma2d::Suspend();
    // From, To, BlockIndex, FromSz
    MCU_BlockIndex += Jpeg::pConvert_Function((uint8_t*)JMcuBuf.BufToProcess, (uint8_t*)FrameBuf1, MCU_BlockIndex, JMcuBuf.DataSzToProcess);

    if(MCU_BlockIndex == Jpeg::Conf.ImageMCUsCnt) return retvOk;
    else return retvInProgress;
}
#endif

#if 1 // ========================== Audio playback =============================
void StartAudio() {

}

void StopAudio() {

}

void ProcessAudioBuf() {

}
#endif

void DmaJpegOutCB(void *p, uint32_t flags) {
//    PrintfI("OutIRQ\r");
    chSysLockFromISR();
    MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdNewMCUBufReady));
    chSysUnlockFromISR();
}

void OnJpegConvEndI() { MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdEndDecode)); }

// DMA Tx Completed IRQ
extern "C"
void DmaSAITxIrq(void *p, uint32_t flags) {
    chSysLockFromISR();
//    PCurBuf = (PCurBuf == &Buf1)? &Buf2 : &Buf1;
//    TransmitBuf(PCurBuf);
//    MsgQSnd.SendNowOrExitI(SndMsg_t(sndcmdPrepareNextBuf));
    chSysUnlockFromISR();
}

#if 1 // ============================== File ===================================
uint8_t ProcessHDRL(int32_t HdrlSz) {
    avimainheader_t AviMainHdr;
    if(AviMainHdr.Read() != retvOk) return retvFail;
    HdrlSz -= AviMainHdr.cb + 8;
    FrameRate = TIME_US2I(AviMainHdr.dwMicroSecPerFrame);
    Printf("FrameRate_st: %u\r", FrameRate);
//    AviMainHdr.Print();
    // Read streams headers
    ChunkHdr_t ListHdr;
    while(HdrlSz > 0) {
        // Read LIST strl
        if(ListHdr.ReadNext() != retvOk) return retvFail;
        HdrlSz -= ListHdr.ckSize + 8;
        if(ListHdr.IsList()) {
            ListHdr.ReadType();
//            ListHdr.PrintWType();
            if(ListHdr.Type == FOURCC('s','t','r','l')) {
                // Read chunks inside the list
                int32_t ListSz = ListHdr.ckSize;
                ChunkHdr_t Hdr;
                avistreamheader_t StreamHdr;
                while(ListSz > 0) {
                    if(Hdr.ReadNext() != retvOk) break;
                    ListSz -= Hdr.ckSize + 8;
//                    Hdr.Print();
                    if(Hdr.ckID == FOURCC('s','t','r','h')) {
                        f_lseek(&ifile, f_tell(&ifile) - 8);
                        if(StreamHdr.Read() != retvOk) return retvFail;
//                        StreamHdr.Print();
                        if(StreamHdr.fccType == FOURCC('v','i','d','s') and StreamHdr.fccHandler != FOURCC('M','J','P','G') ) {
                            Printf("Unsupported video: %4S\r", StreamHdr.fccHandler);
                            return retvFail;
                        }
                    }
                    else if(Hdr.ckID == FOURCC('s','t','r','f')) { // Format
                        if(StreamHdr.fccType == FOURCC('a','u','d','s')) {
                            WAVEFORMATEX WFormat;
                            if(WFormat.Read() != retvOk) return retvFail;
//                            WFormat.Print();
                            if(WFormat.wFormatTag == 1) { // WAVE_FORMAT_PCM = 0x0001
                                Printf("Audio Fs: %u SmpPS\r", WFormat.nSamplesPerSec);
                                Codec.SetupSampleRate(WFormat.nSamplesPerSec);
                                Codec.SetMasterVolume(0); // max
                                Codec.SetSpeakerVolume(0); // max
                            }
                            else {
                                Printf("Unsupported audio\r");
                                return retvFail;
                            }
                        }
                    }
                } // while(ListSz > 0)
            } // if strl
        } // IsList
//        else ListHdr.Print();
    } // while(HdrlSz > 0)
    return retvOk;
}

#endif

static THD_WORKING_AREA(waVideoThd, 1024);
__noreturn
static void VideoThd(void *arg) {
    chRegSetThreadName("Video");
    systime_t FrameStart = 0;
    while(true) {
        VideoMsg_t Msg = MsgQVideo.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case vcmdStart:
                while(!VFileReader.VBuf and !VFileReader.IsLastFrame) chThdSleepMilliseconds(7);
                if(VFileReader.IsLastFrame) MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
                else {
                    StartFrameDecoding();
                    VFileReader.PrepareNextFrame();
                }
                break;

            case vcmdStop:
                Jpeg::Stop();
                StopAudio();
                CloseFile(&ifile);
                Printf("End\r");
                break;

            case vcmdNewMCUBufReady:
                JMcuBuf.Switch(); // Switch buf and continue reception
                JMcuBuf.PrepareToFill();
                ProcessMcuBuf();  // Process what received
                break;

            case vcmdEndDecode: {
                JMcuBuf.Switch();
                ProcessMcuBuf();
                // Delay before frame show
                sysinterval_t Elapsed = chVTTimeElapsedSinceX(FrameStart);
//                Printf("e %u\r", Elapsed);
                if(Elapsed < FrameRate) chThdSleep(FrameRate - Elapsed);
                FrameStart = chVTGetSystemTimeX();
                MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
            } break;

            default: break;
        } // switch
    } // while true
}

#if 1 // ============================= Top Level ===============================
namespace Avi {

void Init() {
//    VFileBuf.Init();
//    BigBuf = (uint32_t*)malloc(BIGBUF_SZ);
    VFileReader.Init();
    JPEG_InitColorTables(); // Init The JPEG Color Look Up Tables used for YCbCr to RGB conversion
    Jpeg::Init(DmaJpegOutCB, OnJpegConvEndI);

    // Create and start threads
    MsgQVideo.Init();

    chThdCreateStatic(waVideoThd, sizeof(waVideoThd), HIGHPRIO, (tfunc_t)VideoThd, NULL);
}

uint8_t Start(const char* FName) {
    uint8_t Rslt = TryOpenFileRead(FName, &ifile);
    if(Rslt != retvOk) return Rslt;
    Rslt = retvFail;
    // Check header
    ChunkHdr_t ckHdr;
    if(ckHdr.ReadNext() != retvOk) goto End;
//    ckHdr.Print();
    if(!ckHdr.IsRiff()) { Printf("BadHdr\r"); goto End; }
    ckHdr.ReadType();
    if(!ckHdr.TypeIsAVI()) { Printf("BadContent\r"); goto End; }
    ckHdr.ckSize = 0; // To read next chunk, not jumping to end of file
    // Analyze: read all chunks and lists
    while(true) {
        if(ckHdr.ReadNext() != retvOk) break;
        if(ckHdr.IsList()) {
            ckHdr.ReadType();
//            ckHdr.PrintWType();
            // Process hdrl
            if(ckHdr.TypeIsHDRL()) {
                uint32_t N = f_tell(&ifile);
                if(ProcessHDRL(ckHdr.ckSize) != retvOk) return retvFail;
                f_lseek(&ifile, N);
            }
            // Process movi
            if(ckHdr.TypeIsMOVI()) {
                VFileReader.Start(ckHdr.ckSize);
                MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
                return retvOk; // movi chunk found
            }
        }
//        else ckHdr.Print();

    } // while true
    End:
    return Rslt;
}

void Stop() {
    MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
}

} // namespace
#endif


