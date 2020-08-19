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

    uint8_t ReadData(uint32_t* Buf) {
        uint32_t *ReadSz = 0;
        if(f_tell(&ifile) != DataLoc) f_lseek(&ifile, DataLoc);
        if(f_read(&ifile, Buf, ckSize, ReadSz) == FR_OK) return retvOk;
        else return retvFail;
    }

    void Print() { Printf("%4S; Sz=%u\r", &ckID, ckSize); }
    bool IsVideo() { return ckID == FOURCC('0','0','d','c'); }
    bool IsAudio() { return ckID == FOURCC('0','1','w','b'); }
private:
    uint32_t NextLoc = 0, DataLoc = 0;
};

#define AUBUF_SZ    4096
#define AUBUF_CNT   (AUBUF_SZ / 4)

//class {
//public:
//    uint32_t Buf1[AUBUF_CNT];
//    uint32_t Buf2[AUBUF_CNT];
////    uint32_t *Buf1, *Buf2;
//} AuBufs;// __attribute__ ((section ("DATA_RAM")));

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

//#define IN_VBUF_SZ      0xA000UL // Packed frame must fit here
//#define IN_VBUF_SZ32    (IN_VBUF_SZ / 4)

#define IN_VBUF_SZ      (40000UL)
#define IN_VBUF_SZ32    (IN_VBUF_SZ / 4)


static THD_WORKING_AREA(waFileThd, 1024);
static void FileThd(void *arg);

//__attribute__ ((section ("DATA_RAM")))
//uint32_t VBuf1[IN_VBUF_SZ32] __attribute__((aligned(32)));
//__attribute__ ((section ("DATA_RAM")))
//uint32_t VBuf2[IN_VBUF_SZ32] __attribute__((aligned(32)));

//uint32_t *VBuf1, *VBuf2;

//uint8_t ReadNextAudio() {
//    systime_t start = chVTGetSystemTimeX();
//    while(true) {
//        if(AuCk.ReadHdr() != retvOk) break;
//        if(AuCk.IsAudio()) { // found
//            if(AuCk.ckSize > AUBUF_SZ) {
//                Printf("Too large Adata\r");
//                break;
//            }
//            AuBuf = (AuBuf == AuBufs.Buf1)? AuBufs.Buf2 : AuBufs.Buf1;
//            // Read data
//            if(AuCk.ReadData(AuBuf, &AuSz) != retvOk) break;
//            sysinterval_t ela = chVTTimeElapsedSinceX(start);
//            if(ela > 418) Printf("Au %u *********\r", ela);
//            else Printf("Au %u\r", ela);
//            return retvOk;
//        }
//    } // while true
//    return retvFail;
//}

//template <typename T, uint32_t Sz>
//class MetaBuf_t {
//private:
//    uint32_t IFullSlotsCount=0;
//    T IBuf[Sz];
//public:
//    T *PRead = IBuf, *PWrite=IBuf;
//
//    void MoveReadPtr() {
//        if(++PRead > (IBuf + Sz - 1)) PRead = IBuf;     // Circulate buffer
//        if(IFullSlotsCount) IFullSlotsCount--;
//    }
//
//    void MoveWritePtr() {
//        if(++PWrite > (IBuf + Sz - 1)) PWrite = IBuf;   // Circulate buffer
//        if(IFullSlotsCount < Sz) IFullSlotsCount++;
//    }
//
//    bool IsEmpty() { return (IFullSlotsCount == 0); }
//    bool IsFull()  { return (IFullSlotsCount == Sz); }
//    uint32_t GetFullCount()  { return IFullSlotsCount; }
//
//    void Flush() {
//        PWrite = IBuf;
//        PRead = IBuf;
//    }
//};

struct SzPtr_t {
    uint16_t Sz;
    uint16_t N;
    uint32_t *ptr;
    SzPtr_t& operator = (const SzPtr_t &Right) {
        Sz = Right.Sz;
        N = Right.N;
        ptr = Right.ptr;
        return *this;
    }

} __attribute__((packed));

class VFileReader_t {
private:
    uint32_t IVBuf[IN_VBUF_SZ32] __attribute__((aligned(32)));
    CircBuf_t<SzPtr_t, 32> IVMeta;

    thread_t *ThdPtr;
    Chunk_t VCk, AuCk;
    uint32_t EndLoc = 0;

    uint32_t rneMax = 0;

//    uint8_t ReadNextFrame() {
//        uint32_t *NextBuf = VBuf;
//        VBuf = nullptr;
//        systime_t start = chVTGetSystemTimeX();
//        while(true) {
//            if(VCk.ReadHdr() != retvOk) break;
////            Printf("rne %u  ", chVTTimeElapsedSinceX(start));
////            VCk.Print();
//            if(VCk.IsVideo()) { // found
//                if(VCk.ckSize > IN_VBUF_SZ) {
//                    Printf("Too large data\r");
//                    return retvFail;
//                }
//                // Read data
//                NextBuf = (NextBuf == VBuf1)? VBuf2 : VBuf1;
////                systime_t start = chVTGetSystemTimeX();
//
//                if(VCk.ReadData(NextBuf, &VSz) != retvOk) break;
////                Printf("%A\r", NextBuf, 128, ' ');
//                sysinterval_t ela = chVTTimeElapsedSinceX(start);
//                if(ela > 418) Printf("rne %u *********\r", ela);
//                if(ela > rneMax) rneMax = ela;
//                Printf("rne %u %u\r", ela, rneMax);
//
////                SCB_CleanDCache();
////                SCB_CleanDCache_by_Addr(NextBuf, VSz);
//
////                cacheBufferFlush(NextBuf, VSz);
////                __DSB();
////                __ISB();
//                VBuf = NextBuf;
////                VBuf = VCk.pData;
////                VSz = VCk.ckSize;
//                VIndx = 0;
//                return retvOk;
//            }
//        }
//        return retvFail;
//    }
public:
    uint32_t *VBuf = nullptr, VSz = 0;
    uint32_t *AuBuf = nullptr, AuSz = 0;
    bool IsEof = false;

    void Init() {
//        VBuf1 = (uint32_t*)malloc(IN_VBUF_SZ);
//        VBuf2 = (uint32_t*)malloc(IN_VBUF_SZ);

//        AuBufs.Buf1 = (uint32_t*)malloc(AUBUF_SZ);
//        AuBufs.Buf2 = (uint32_t*)malloc(AUBUF_SZ);
        chThdCreateStatic(waFileThd, sizeof(waFileThd), NORMALPRIO, (tfunc_t)FileThd, NULL);
    }

    void Start(uint32_t VideoSz) {
//        SCB_DisableDCache();
//        EndLoc = f_tell(&ifile) + VideoSz;
        // Video
        VCk.MoveTo(f_tell(&ifile));
        IVMeta.Flush();
        IVMeta.PWrite->Sz = 0;
        IVMeta.PWrite->ptr = IVBuf;
        IsEof = false;
        // Audio
//        AuCk.MoveTo(f_tell(&ifile));
//        AuBuf = nullptr;
//        if(ReadNextAudio() == retvOk) {
//            Codec.TransmitBuf(AuBuf, AuSz / 2);
//            chSysLock();
//            PrepareNextAudioI();
//            chSysUnlock();
//        }

        chThdResume(&ThdPtr, MSG_OK);
    }

    void PrepareNextFrame() {
        chThdResume(&ThdPtr, MSG_OK);
    }

    void PrepareNextAudioI() {
//        MsgQFile.SendNowOrExitI(FileMsg_t(fcmdReadNextAudio));
    }

    uint8_t GetNextFrame() {
        if(IVMeta.IsEmpty()) return retvEmpty;
        chSysLock();
        PrintfI("\r R%u; N%u Sz%u p%u;", IVMeta.GetFullCount(), IVMeta.PRead->N, IVMeta.PRead->Sz, (IVMeta.PRead->ptr - IVBuf)*4);
        VSz = IVMeta.PRead->Sz;
        VBuf = IVMeta.PRead->ptr;
        IVMeta.MoveReadPtr();
        PrintfI(" %u\r", IVMeta.GetFullCount());
        chSysUnlock();
        return retvOk;
    }

    void OnFrameSentI() {

    }

    void WriteFrame() {
//        Printf("W %u: %u; ", (wPtrV - IVBuf), VCk.ckSize);
        VCk.ReadData(IVMeta.PWrite->ptr);
        chSysLock();
        IVMeta.PWrite->Sz = VCk.ckSize;
        IVMeta.PWrite->N = VCk.N++;
        VCk.AwaitsReading = false;
        // Prepare next write
        uint32_t *ptr = IVMeta.PWrite->ptr;
        IVMeta.MoveWritePtr();
        IVMeta.PWrite->ptr = ptr + (VCk.ckSize + 3) / 4;
        chSysUnlock();
    }

    uint8_t PutFrameIfPossible() {
        chSysLock();
        PrintfI("W%u; N%u Sz%u p%u; ", IVMeta.GetFullCount(), VCk.N, VCk.ckSize, (IVMeta.PWrite->ptr - IVBuf)*4);
        if(IVMeta.IsFull()) {
            chSysUnlock();
            return retvOverflow;
        }
        else if(IVMeta.IsEmpty()) {
            IVMeta.PWrite->ptr = IVBuf;
            chSysUnlock();
            WriteFrame();
            Printf(" FC %u\r", IVMeta.GetFullCount());
            return retvOk;
        }
        uint32_t *RPtr = IVMeta.PRead->ptr;
        // Where to write?
        if(RPtr <= IVMeta.PWrite->ptr) {
            uint32_t EndSz = ((IVBuf + IN_VBUF_SZ32) - IVMeta.PWrite->ptr) * 4;
            PrintfI(" Esz: %d", EndSz);
            if(EndSz >= VCk.ckSize) { // There is enough space in end of buf
                chSysUnlock();
                WriteFrame();
                Printf(" FC %u\r", IVMeta.GetFullCount());
                return retvOk;
            }
            else { // Not enough space in end, move to start
                PrintfI(" Mv");
                IVMeta.PWrite->ptr = IVBuf;
            }
        }
        if(IVMeta.PWrite->ptr <= RPtr) {
            uint32_t StartSz = (RPtr - IVMeta.PWrite->ptr) * 4;
            PrintfI(" SSz: %d", StartSz);
            if(StartSz >= VCk.ckSize) {
                chSysUnlock();
                WriteFrame();
                Printf(" FC %u\r", IVMeta.GetFullCount());
                return retvOk;
            }
//            else Printf(" NES");
        }
        chSysUnlock();
        Printf(" OVF FC %u\r", IVMeta.GetFullCount());
        return retvOverflow;
    }

    __noreturn
    void ITask() {
        while(true) {
            chSysLock();
            chThdSuspendS(&ThdPtr);
            chSysUnlock();

            uint8_t VRslt = retvOk;
            while(VRslt == retvOk) {
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
                    } // if readhdr
                    else IsEof = true;
                }
            } // while ok

//            FileMsg_t Msg = MsgQFile.Fetch(TIME_INFINITE);
//            switch(Msg.Cmd) {
//                case fcmdReadNextFrame:
//                    IsLastFrame = (ReadNextFrame() != retvOk);
//                    break;
//                case fcmdReadNextAudio:
//                    ReadNextAudio();
//                    break;
//                default: break;
//            }
        } // while true
    }
} VFileReader  __attribute__((aligned(32)));

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

void StartFrameDecoding(uint32_t *Buf, uint32_t Sz) {
    Jpeg::Stop();
    MCU_BlockIndex = 0;
    Jpeg::PrepareToStart(Buf, (Sz + 3) / 4);
    JMcuBuf.PrepareToFill();
    Jpeg::Start();
}

uint8_t ProcessMcuBuf() {
    Dma2d::Suspend();
    // From, To, BlockIndex, FromSz
    MCU_BlockIndex += Jpeg::pConvert_Function((uint8_t*)JMcuBuf.BufToProcess, (uint8_t*)FrameBuf1, MCU_BlockIndex, JMcuBuf.DataSzToProcess);
//    cacheBufferFlush(FrameBuf1, LBUF_SZ / 4);

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

#if 1 // ========================= Callbacks and IRQs ==========================
void DmaJpegInCB(void *p, uint32_t flags) {
//    PrintfI("InIRQ\r");
    chSysLockFromISR();
    VFileReader.OnFrameSentI();
    chSysUnlockFromISR();
}

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
    Codec.TransmitBuf(VFileReader.AuBuf, VFileReader.AuSz / 2);
    VFileReader.PrepareNextAudioI();
    chSysUnlockFromISR();
}
#endif

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
//    uint32_t n = 0;
    while(true) {
        VideoMsg_t Msg = MsgQVideo.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case vcmdStart:
                while(VFileReader.GetNextFrame() != retvOk and !VFileReader.IsEof) chThdSleepMilliseconds(7);
                if(VFileReader.IsEof) MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
                else {
//                    Printf("\r%u: %u %u\r", n++, VFileReader.VSz, (VFileReader.VBuf - (uint32_t*)&VFileReader) * 4);
                    StartFrameDecoding(VFileReader.VBuf, VFileReader.VSz);
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
    Jpeg::Init(DmaJpegInCB, DmaJpegOutCB, OnJpegConvEndI);

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


