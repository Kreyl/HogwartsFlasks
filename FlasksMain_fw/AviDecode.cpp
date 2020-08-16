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

//__attribute__ ((section ("DATA_RAM")))

FIL ifile;

#if 1 // ============================ AVI headers ==============================
#define IN_VBUF_SZ      0xA000UL // Packed frame must fit here
#define IN_VBUF_SZ32    (IN_VBUF_SZ / 4)

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
            if(f_eof(&ifile)) return retvEndOfFile;
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

class VideoChunk_t {
public:
    uint32_t ckID = 0; // }
    uint32_t Sz = 0;   // } Will be read together
    uint32_t Buf[IN_VBUF_SZ32];
    uint32_t NextLoc = 0;

    uint8_t ReadNext() {
        f_lseek(&ifile, NextLoc); // Goto next chunk
        while(true) {
            uint32_t BytesRead = 0;
            // Read next header
            if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
//            Printf("%4S; Sz=%u\r", &ckID, Sz);
            if(ckID == FOURCC('0','0','d','c')) { // found
                if(Sz > IN_VBUF_SZ) {
                    Printf("Too large data\r");
                    return retvFail;
                }
                // Read data
                if(f_read(&ifile, Buf, Sz, &BytesRead) == FR_OK and BytesRead == Sz) {
//                    cacheBufferFlush(Buf, Sz);
                    // Prepare next loc
                    NextLoc = f_tell(&ifile);
                    if(NextLoc & 1) NextLoc++;
                    return retvOk;
                }
            }
            else {
                if(Sz & 1) Sz++;
                f_lseek(&ifile, f_tell(&ifile) + Sz);
            }
        }
        return retvFail;
    }
} VideoChunk;

//__attribute__ ((section ("DATA_RAM")))
class AudioChunk_t {
public:
    uint32_t ckID = 0; // }
    uint32_t Sz = 0;   // } Will be read together
    uint32_t Buf[IN_AUBUF_SZ32];
    uint32_t NextLoc = 0;

    uint8_t ReadNext() {
        f_lseek(&ifile, NextLoc); // Goto next chunk
        while(true) {
            uint32_t BytesRead = 0;
            // Read next header
            if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
//            Printf("%4S; Sz=%u\r", &ckID, Sz);
            if(ckID == FOURCC('0','1','w','b')) { // found
                if(Sz > IN_AUBUF_SZ) {
                    Printf("Too large data\r");
                    return retvFail;
                }
                // Read data
//                if(f_read(&ifile, Buf, Sz, &BytesRead) == FR_OK and BytesRead == Sz) {
////                    cacheBufferFlush(Buf, Sz);
//                    // Prepare next loc
//                    NextLoc = f_tell(&ifile);
//                    if(NextLoc & 1) NextLoc++;
                    return retvOk;
//                }
            }
            else {
                if(Sz & 1) Sz++;
                f_lseek(&ifile, f_tell(&ifile) + Sz);
            }
        }
        return retvFail;
    }
} AudioChunk;

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

class OutBuf_t {
private:
    uint8_t *pBuf1, *pBuf2;
public:
    void Init() {
        pBuf1 = (uint8_t*)malloc(OUTBUF_SZ);
        pBuf2 = (uint8_t*)malloc(OUTBUF_SZ);
    }
    uint8_t **CurrBuf = &pBuf1;
    void Switch() {
        if(CurrBuf == &pBuf1) CurrBuf = &pBuf2;
        else CurrBuf = &pBuf1;
    }
} Outbuff;

void StartFrameDecoding() {
    Jpeg::Stop();
    MCU_BlockIndex = 0;
    Jpeg::PrepareToStart(VideoChunk.Buf, (VideoChunk.Sz + 3) / 4);
    JMcuBuf.PrepareToFill();
    Jpeg::Start();
}

uint8_t ProcessMcuBuf() {
    Dma2d::Suspend();
    // From, To, BlockIndex, FromSz
    MCU_BlockIndex += Jpeg::pConvert_Function((uint8_t*)JMcuBuf.BufToProcess, *Outbuff.CurrBuf, MCU_BlockIndex, JMcuBuf.DataSzToProcess);
    Dma2d::Resume();
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

static THD_WORKING_AREA(waVideoThd, 1024);
__noreturn
static void VideoThd(void *arg) {
    chRegSetThreadName("Video");
    systime_t FrameStart = 0;
//    uint32_t fcnt = 0;
//    uint32_t sum = 0;
    while(true) {
        VideoMsg_t Msg = MsgQVideo.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case vcmdStart:
                // Try to get next frame
                if(VideoChunk.ReadNext() == retvOk and AudioChunk.ReadNext() == retvOk) {
                    StartFrameDecoding();
                }
                else MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
                break;

            case vcmdStop:
                Jpeg::Stop();
                StopAudio();
                CloseFile(&ifile);
                break;

            case vcmdNewMCUBufReady:
                JMcuBuf.Switch(); // Switch buf and continue reception
                JMcuBuf.PrepareToFill();
                ProcessMcuBuf();  // Process what received
                break;

            case vcmdEndDecode:
                JMcuBuf.Switch();
                if(ProcessMcuBuf() == retvOk) {
                    Jpeg::GetInfo();
                    // Copy RGB decoded Data to the display FrameBuffer
                    uint32_t width_offset = 0;
                    uint32_t xPos = (LCD_WIDTH - Jpeg::Conf.ImageWidth) / 2;
                    uint32_t yPos = (LCD_HEIGHT - Jpeg::Conf.ImageHeight) / 2;

                    if(Jpeg::Conf.ChromaSubsampling == JPEG_420_SUBSAMPLING) {
                        if((Jpeg::Conf.ImageWidth % 16) != 0) width_offset = 16 - (Jpeg::Conf.ImageWidth % 16);
                    }
                    if(Jpeg::Conf.ChromaSubsampling == JPEG_422_SUBSAMPLING) {
                        if((Jpeg::Conf.ImageWidth % 16) != 0) width_offset = 16 - (Jpeg::Conf.ImageWidth % 16);
                    }
                    if(Jpeg::Conf.ChromaSubsampling == JPEG_444_SUBSAMPLING) {
                        if((Jpeg::Conf.ImageWidth % 8) != 0) width_offset = (Jpeg::Conf.ImageWidth % 8);
                    }

                    // Delay before frame show
                    sysinterval_t Elapsed = chVTTimeElapsedSinceX(FrameStart);
                    Printf("e %u\r", Elapsed);
//                    sum += Elapsed;
//                    fcnt++;
//                    Printf("ea %u\r", sum / fcnt);
                    if(Elapsed < FrameRate) {
                        chThdSleep(FrameRate - Elapsed);
                    }
                    FrameStart = chVTGetSystemTimeX();
                    Dma2d::CopyBuffer((uint32_t *)(*Outbuff.CurrBuf), (uint32_t *)FrameBuf1, xPos, yPos, Jpeg::Conf.ImageWidth, Jpeg::Conf.ImageHeight, width_offset);
                    Outbuff.Switch();
                    MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
                }
                break;

            default: break;
        } // switch
    } // while true
}

void OnJpegConvEndI() { MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdEndDecode)); }

#if 1 // ============================= Top Level ===============================
namespace Avi {

void Init() {
    Outbuff.Init();
    JPEG_InitColorTables(); // Init The JPEG Color Look Up Tables used for YCbCr to RGB conversion
    Jpeg::Init(DmaJpegOutCB, OnJpegConvEndI);

    Dma2d::Init();

    // Create and start thread
    MsgQVideo.Init();
    chThdCreateStatic(waVideoThd, sizeof(waVideoThd), HIGHPRIO, (tfunc_t)VideoThd, NULL);
}

uint8_t ProcessHDRL(int32_t HdrlSz) {
    avimainheader_t AviMainHdr;
    if(AviMainHdr.Read() != retvOk) return retvFail;
    HdrlSz -= AviMainHdr.cb + 8;
    FrameRate = TIME_US2I(AviMainHdr.dwMicroSecPerFrame);
    Printf("FrameRate_st: %u\r", FrameRate);
    AviMainHdr.Print();
    // Read streams headers
    ChunkHdr_t ListHdr;
    while(HdrlSz > 0) {
        // Read LIST strl
        if(ListHdr.ReadNext() != retvOk) return retvFail;
        HdrlSz -= ListHdr.ckSize + 8;
        if(ListHdr.IsList()) {
            ListHdr.ReadType();
            ListHdr.PrintWType();
            if(ListHdr.Type == FOURCC('s','t','r','l')) {
                // Read chunks inside the list
                int32_t ListSz = ListHdr.ckSize;
                ChunkHdr_t Hdr;
                avistreamheader_t StreamHdr;
                while(ListSz > 0) {
                    if(Hdr.ReadNext() != retvOk) break;
                    ListSz -= Hdr.ckSize + 8;
                    Hdr.Print();
                    if(Hdr.ckID == FOURCC('s','t','r','h')) {
                        f_lseek(&ifile, f_tell(&ifile) - 8);
                        if(StreamHdr.Read() != retvOk) return retvFail;
                        StreamHdr.Print();
                        if(StreamHdr.fccType == FOURCC('v','i','d','s') and StreamHdr.fccHandler != FOURCC('M','J','P','G') ) {
                            Printf("Unsupported video format\r");
                            return retvFail;
                        }
                    }
                    else if(Hdr.ckID == FOURCC('s','t','r','f')) { // Format
                        if(StreamHdr.fccType == FOURCC('a','u','d','s')) {
                            WAVEFORMATEX WFormat;
                            if(WFormat.Read() != retvOk) return retvFail;
                            WFormat.Print();
                            if(WFormat.wFormatTag != 1) { // WAVE_FORMAT_PCM = 0x0001
                                Printf("Unsupported audio format\r");
                                return retvFail;
                            }
                        }
                    }
                } // while(ListSz > 0)
            } // if strl
        } // IsList
        else ListHdr.Print();
    } // while(HdrlSz > 0)
    return retvOk;
}

uint8_t Start(const char* FName) {
    uint8_t Rslt = TryOpenFileRead(FName, &ifile);
    if(Rslt != retvOk) return Rslt;
    Rslt = retvFail;
    // Check header
    ChunkHdr_t ckHdr;
    if(ckHdr.ReadNext() != retvOk) goto End;
    ckHdr.Print();
    if(!ckHdr.IsRiff()) { Printf("BadHdr\r"); goto End; }
    ckHdr.ReadType();
    if(!ckHdr.TypeIsAVI()) { Printf("BadContent\r"); goto End; }
    ckHdr.ckSize = 0; // To read next chunk, not jumping to end of file
    // Analyze: read all chunks and lists
    while(true) {
        if(ckHdr.ReadNext() != retvOk) break;
        if(ckHdr.IsList()) {
            ckHdr.ReadType();
            ckHdr.PrintWType();
            // Process hdrl
            if(ckHdr.TypeIsHDRL()) {
                uint32_t N = f_tell(&ifile);
                if(ProcessHDRL(ckHdr.ckSize) != retvOk) return retvFail;
                f_lseek(&ifile, N);
            }
            // Process movi
            if(ckHdr.TypeIsMOVI()) {
                VideoChunk.NextLoc = f_tell(&ifile);
                VideoChunk.Sz = 0;
                AudioChunk.NextLoc = f_tell(&ifile);
                AudioChunk.Sz = 0;
                // Everything is prepared to start
                StartAudio();
                MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
                return retvOk; // movi chunk found
            }
        }
        else ckHdr.Print();

    } // while true
    End:
    return Rslt;
}

void Stop() {
    MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
}

} // namespace
#endif

