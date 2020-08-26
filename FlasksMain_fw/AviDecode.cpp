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

static FIL ifile;
extern CS42L52_t Codec;
uint32_t MCU_BlockIndex = 0;
sysinterval_t FrameRate = 0;
systime_t FrameStart = 0;

namespace Audio {
bool IsIdle;
void Start();
};

#if 1 // ============================ AVI headers ==============================
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

uint8_t ProcessHDRL(int32_t HdrlSz) {
    avimainheader_t AviMainHdr;
    if(AviMainHdr.Read() != retvOk) return retvFail;
    HdrlSz -= AviMainHdr.cb + 8;
    FrameRate = TIME_US2I(AviMainHdr.dwMicroSecPerFrame);
//    FrameRate = 405; // Debug
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
                            Printf("Unsupported video: %4S\r", &StreamHdr.fccHandler);
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
                                Printf("Unsupported audio: %u\r", WFormat.wFormatTag);
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

#if 1 // ========================== File buffering =============================
#define RAM0_TOTAL_SZ   (0x5F000UL) // Const
#define IN_VBUF_SZ      0x30000UL
#define IN_VBUF_SZ32    (IN_VBUF_SZ / 4)
#define FRAMES_CNT_MAX  18UL

#define AUBUF_SZ        0x20000
#define AUBUF_THRESHOLD 12UL

static uint8_t  IABuf[AUBUF_SZ]     __attribute__((aligned(32), section (".srambuf")));
static uint32_t IVBuf[IN_VBUF_SZ32] __attribute__((aligned(32), section (".srambuf")));

static THD_WORKING_AREA(waFileThd, 1024);
static void FileThd(void *arg);

class Chunk_t {
public:
    uint32_t ckID;
    uint32_t ckSize = 0; //  includes the size of listType plus the size of listData
    bool AwaitsReading = false;
    uint16_t N = 0;

    void MoveTo(uint32_t NewLoc) { NextLoc = NewLoc; }

    uint8_t ReadHdr() {
        if(f_tell(&ifile) != NextLoc) f_lseek(&ifile, NextLoc);
        if(f_eof(&ifile)) return retvEndOfFile;
        uint32_t BytesRead = 0;
        if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
        if(ckSize & 1) ckSize++;
        NextLoc = f_tell(&ifile) + ckSize;
        DataLoc = f_tell(&ifile);
        return retvOk;
    }

    uint8_t ReadData(void* Buf) {
//        systime_t Start = chVTGetSystemTimeX();
        uint32_t *ReadSz = 0;
        if(f_tell(&ifile) != DataLoc) f_lseek(&ifile, DataLoc);
        uint8_t Rslt = (f_read(&ifile, Buf, ckSize, ReadSz) == FR_OK)? retvOk : retvFail;
//        Printf("rt %u\r", chVTTimeElapsedSinceX(Start));
        return Rslt;
    }

    void Print() { Printf("%4S; Sz=%u\r", &ckID, ckSize); }
    bool IsVideo() { return ckID == FOURCC('0','0','d','c'); }
    bool IsAudio() { return ckID == FOURCC('0','1','w','b'); }
//private:
    uint32_t NextLoc = 0, DataLoc = 0;
};

class AuBuf_t {
private:
    uint8_t *PRead, *PWrite, *JumpPtr;
    uint32_t IFullSlotsCount=0;

    void WriteData(Chunk_t &ACk) {
//        SCB_CleanDCache();
        ACk.ReadData(PWrite);
        //SCB_InvalidateDCache();
        chSysLock();
        ACk.AwaitsReading = false;
        PWrite += ACk.ckSize;
        if(PWrite >= (IABuf + AUBUF_SZ)) PWrite = IABuf;
        IFullSlotsCount += ACk.ckSize;
        chSysUnlock();
    }
public:
    void Init() {
        PRead = IABuf;
        PWrite = IABuf;
        JumpPtr = IABuf + AUBUF_SZ;
    }

    uint8_t Put(Chunk_t &ACk) {
        chSysLock();
        // Where to write?
        if((PRead < PWrite) or (PRead == PWrite and IFullSlotsCount == 0)) {
            JumpPtr = IABuf + AUBUF_SZ;
            uint32_t EndSz = (IABuf + AUBUF_SZ) - PWrite;
            if(EndSz >= ACk.ckSize) { // There is enough space in end of buf
                chSysUnlock();
                WriteData(ACk);
                return retvOk;
            }
            else {
                JumpPtr = PWrite;
                PWrite = IABuf;
            }
        }
        if(PWrite <= PRead) {
            uint32_t StartSz = PRead - PWrite;
            if(StartSz >= ACk.ckSize) {
                chSysUnlock();
                WriteData(ACk);
                return retvOk;
            }
        }
        PrintfI("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ AOvf\r");
        chSysUnlock();
        return retvFail;
    }

    uint8_t* GetRPtrAndMove(uint32_t *pSz) {
        if(IFullSlotsCount == 0) {
            *pSz = 0;
            return nullptr;
        }
        uint8_t *r = PRead;
        // How many slots left in buffer?
        uint32_t Sz = (PWrite > PRead)? (PWrite - PRead) : (JumpPtr - PRead);
        // Use min value
        if(Sz > *pSz) Sz = *pSz;
        else if(Sz < *pSz) *pSz = Sz;
        // Move PRead
        PRead += Sz;
        if(PRead >= JumpPtr) PRead = IABuf;
        IFullSlotsCount -= Sz;
        return r;
    }
    uint32_t GetFullCount()  { return IFullSlotsCount; }
    uint32_t GetEmptyCount() { return AUBUF_SZ-IFullSlotsCount; }
} AuBuf;

template <typename T, uint32_t Sz>
class MetaBuf_t {
private:
    uint32_t IFullSlotsCount=0;
    T IBuf[Sz];
public:
    T *PRead = IBuf, *PWrite=IBuf;

    void MoveReadPtr() {
        if(++PRead > (IBuf + Sz - 1)) PRead = IBuf;     // Circulate buffer
        if(IFullSlotsCount) IFullSlotsCount--;
    }

    void MoveWritePtr() {
        if(++PWrite > (IBuf + Sz - 1)) PWrite = IBuf;   // Circulate buffer
        if(IFullSlotsCount < Sz) IFullSlotsCount++;
    }

    bool IsEmpty() { return (IFullSlotsCount == 0); }
    bool Has2EmptySlots() { return IFullSlotsCount <= (Sz - 2); }
    uint32_t GetFullCount()  { return IFullSlotsCount; }

    void Flush() {
        PWrite = IBuf;
        PRead = IBuf;
    }
};

struct SzPtr_t {
    uint16_t Sz;
    uint16_t N;
    uint32_t *ptr;
} __attribute__((packed));

class VFileReader_t {
private:
    MetaBuf_t<SzPtr_t, (FRAMES_CNT_MAX + 2UL)> IVMeta;
    thread_t *ThdPtr;
    Chunk_t VCk;
    uint32_t EndLoc = 0;

    void WriteFrame() {
//        Printf("W %u: %u; ", (wPtrV - IVBuf), VCk.ckSize);
        VCk.ReadData(IVMeta.PWrite->ptr);
        chSysLock();
        if(VCk.ckSize & 2) VCk.ckSize += 2;
        IVMeta.PWrite->Sz = VCk.ckSize;
        IVMeta.PWrite->N = VCk.N++;
        VCk.AwaitsReading = false;
        // Prepare next write
        uint32_t *ptr = IVMeta.PWrite->ptr;
        IVMeta.MoveWritePtr();
        IVMeta.PWrite->ptr = ptr + VCk.ckSize / 4;
        chSysUnlock();
    }

    uint8_t PutFrameIfPossible() {
        chSysLock();
        if(!IVMeta.Has2EmptySlots()) {
            chSysUnlock();
            return retvOverflow;
        }
        uint32_t *RPtr = IVMeta.PRead->ptr;
        // Where to write?
        if((RPtr < IVMeta.PWrite->ptr) or (RPtr == IVMeta.PWrite->ptr and IVMeta.IsEmpty())) {
            uint32_t EndSz = ((IVBuf + IN_VBUF_SZ32) - IVMeta.PWrite->ptr) * 4;
            if(EndSz >= VCk.ckSize) { // There is enough space in end of buf
                chSysUnlock();
                WriteFrame();
                return retvOk;
            }
            else IVMeta.PWrite->ptr = IVBuf; // Not enough space in end, move to start
        }
        if(IVMeta.PWrite->ptr <= RPtr) {
            uint32_t StartSz = (RPtr - IVMeta.PWrite->ptr) * 4;
            if(StartSz >= VCk.ckSize) {
                chSysUnlock();
                WriteFrame();
                return retvOk;
            }
        }
        chSysUnlock();
        return retvOverflow;
    }

    void PrefillBufs() {
        uint8_t VRslt = retvOk, AuRslt = retvOk;
        while(VCk.ReadHdr() == retvOk and VRslt == retvOk and AuRslt == retvOk) {
            if(VCk.IsVideo()) {
                if(IVMeta.GetFullCount() < FRAMES_CNT_MAX) {
                    VRslt = PutFrameIfPossible();
                }
                else VRslt = retvFail;
            } // if is video
            else if(VCk.IsAudio()) {
                if(AuBuf.GetEmptyCount() > VCk.ckSize) {
                    AuRslt = AuBuf.Put(VCk);
                }
                else AuRslt = retvFail;
            }
        } // while
    }

public:
    uint32_t *VBuf = nullptr, VSz = 0;
    bool IsEof = false;

    uint32_t GetFullCount()  { return IVMeta.GetFullCount(); }

    void Init() {
        chThdCreateStatic(waFileThd, sizeof(waFileThd), NORMALPRIO, (tfunc_t)FileThd, NULL);
    }

    void Start(uint32_t VideoSz) {
//        SCB_DisableDCache();
        Audio::IsIdle = true;
        EndLoc = f_tell(&ifile) + VideoSz;
        // Video
        VCk.MoveTo(f_tell(&ifile));
        IVMeta.Flush();
        IVMeta.PWrite->Sz = 0;
        IVMeta.PWrite->ptr = IVBuf;
        IsEof = false;
        PrefillBufs();
        MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
    }

    uint8_t GetNextFrame() {
        if(IVMeta.IsEmpty()) return retvEmpty;
        chSysLock();
        VSz = IVMeta.PRead->Sz;
        VBuf = IVMeta.PRead->ptr;
        chSysUnlock();
        return retvOk;
    }

    void OnFrameProcessed() {
        chSysLock();
        IVMeta.MoveReadPtr();
        chThdResumeI(&ThdPtr, MSG_OK);
        chSysUnlock();
    }

    void WakeThdI() { chThdResumeI(&ThdPtr, MSG_OK); }

    __noreturn
    void ITask() {
        while(true) {
            chSysLock();
            chThdSuspendS(&ThdPtr);
            chSysUnlock();

            if(f_tell(&ifile) >= EndLoc) {
                IsEof = true;
                continue;
            }
            uint8_t VRslt = retvOk, AuRslt = retvOk;
            while(VRslt == retvOk and AuRslt == retvOk) {
                if(VCk.AwaitsReading) {
                    VRslt = PutFrameIfPossible();
                }
                else {
                    if(VCk.ReadHdr() == retvOk) {
//                        VCk.Print();
                        if(VCk.IsVideo()) {
                            VCk.AwaitsReading = true;
                            VRslt = PutFrameIfPossible();
                        } // if is video
                        else if(VCk.IsAudio()) {
                            AuRslt = AuBuf.Put(VCk);
                        }
                    } // if readhdr
                    else IsEof = true;
                }
            } // while ok
        } // while true
    }
} VFileReader;

__noreturn
static void FileThd(void *arg) {
    chRegSetThreadName("VideoFile");
    VFileReader.ITask();
}
#endif

#if 1 // ========================== Audio playback =============================
namespace Audio {

void OnBufTxEndI() {
    if(VFileReader.IsEof) MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdStop));
    else {
        uint32_t Sz = 4096UL;
        uint8_t* p = AuBuf.GetRPtrAndMove(&Sz);
        if(Sz > 0) {
            Codec.TransmitBuf(p, Sz / 2);
            IsIdle = false;
        }
        else {
            IsIdle = true;
            PrintfI("###########################################################\r");
        }
        VFileReader.WakeThdI();
    } // not EOF
}

void Start() {
    chSysLock();
    OnBufTxEndI();
    chSysUnlock();
}

void Stop() {
    Codec.Stop();
}

}; // Namespace
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

class JMcuBuf_t {
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

void StartFrameDecoding(uint32_t *Buf, uint32_t Sz) {
    Jpeg::Stop();
    MCU_BlockIndex = 0;
    Jpeg::PrepareToStart(Buf, (Sz + 3) / 4);
    JMcuBuf.PrepareToFill();
    Jpeg::Start();
}

uint8_t ProcessMcuBuf() {
    // XXX
    // From, To, BlockIndex, FromSz
    MCU_BlockIndex += Jpeg::pConvert_Function((uint8_t*)JMcuBuf.BufToProcess, (uint8_t*)FrameBuf1, MCU_BlockIndex, JMcuBuf.DataSzToProcess);
    if(MCU_BlockIndex == Jpeg::Conf.ImageMCUsCnt) return retvOk;
    else return retvInProgress;
}
#endif

#if 1 // ========================= Callbacks and IRQs ==========================
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
    Audio::OnBufTxEndI();
    chSysUnlockFromISR();
}
#endif

static THD_WORKING_AREA(waVideoThd, 1024);
__noreturn
static void VideoThd(void *arg) {
    chRegSetThreadName("Video");
//    uint32_t n = 0;
    while(true) {
        VideoMsg_t Msg = MsgQVideo.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case vcmdStart:
                while(VFileReader.GetNextFrame() != retvOk and !VFileReader.IsEof) chThdSleepMilliseconds(7);
                if(VFileReader.IsEof) MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
                else {
                    StartFrameDecoding(VFileReader.VBuf, VFileReader.VSz);
                    if(Audio::IsIdle) Audio::Start();
                }
                break;

            case vcmdStop:
                Jpeg::Stop();
                Audio::Stop();
                CloseFile(&ifile);
                Printf("End\r");
                break;

            case vcmdNewMCUBufReady:
                JMcuBuf.Switch(); // Switch buf and continue reception
                JMcuBuf.PrepareToFill();
                ProcessMcuBuf();  // Process what received
                break;

            case vcmdEndDecode: {
                VFileReader.OnFrameProcessed();
                JMcuBuf.Switch();
                ProcessMcuBuf();
                // Delay before frame show
                sysinterval_t Elapsed = chVTTimeElapsedSinceX(FrameStart);
//                Printf("e %u\r", Elapsed);
                if(Elapsed < FrameRate) {
                    sysinterval_t FDelay = FrameRate - Elapsed;
                    // Increase or decrease delay depending on Video/Audio buffer sizes ratio
//                    uint32_t VCnt = VFileReader.GetFullCount(); // Frames in videobuf
                    uint32_t ACnt =  AuBuf.GetFullCount() / 4096; // "Frames" in audiobuf
//                    Printf("V %2u; A %3u", VCnt, ACnt);
                    Printf("A %3u", ACnt);
                    if(ACnt < AUBUF_THRESHOLD) {
                        FDelay -= 11;
                        Printf("****");
                    }
                    else if(ACnt > AUBUF_THRESHOLD) {
                        FDelay += 11;
                        Printf("####");
                    }
                    PrintfEOL();
                    chThdSleep(FDelay);
                }
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
    VFileReader.Init();
    AuBuf.Init();
    JPEG_InitColorTables(); // Init The JPEG Color Look Up Tables used for YCbCr to RGB conversion
    Jpeg::Init(DmaJpegOutCB, OnJpegConvEndI);
    // Create and start thread
    MsgQVideo.Init();
    chThdCreateStatic(waVideoThd, sizeof(waVideoThd), HIGHPRIO, (tfunc_t)VideoThd, NULL);
}

uint8_t Start(const char* FName, uint32_t FrameN) {
    uint8_t Rslt = TryOpenFileRead(FName, &ifile);
    if(Rslt != retvOk) return Rslt;
    Rslt = retvFail;
    // Check header
    ChunkHdr_t ckHdr;
    if(ckHdr.ReadNext() != retvOk) goto End;
//    ckHdr.Print();
    if(!ckHdr.IsRiff()) { Printf("BadHdr: %4S %u\r", &ckHdr.ckID, ckHdr.ckSize); goto End; }
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
                // Fast forward
                ChunkHdr_t VHdr;
//                while(FrameN > 0) {
//                bool WasV = false;
//                FrameN = 0;
//                uint32_t prevN = 0;
//                while(true) {
//                    if(VHdr.ReadNext() != retvOk) goto End;
//                    VHdr.Print(); chThdSleepMilliseconds(11);
//                    if(VHdr.IsVideo()) {
//                        if(WasV) {
//                            Printf("%u %u\r", FrameN, FrameN-prevN);
//                            prevN = FrameN;
//                        }
//                        WasV = true;
//                        FrameN++;
//                    }
//                    else WasV = false;
////                    if(VHdr.IsVideo()) FrameN--;
////                    if(FrameN == 0) f_lseek(&ifile, f_tell(&ifile) - 8);
//                }

                VFileReader.Start(ckHdr.ckSize);
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


