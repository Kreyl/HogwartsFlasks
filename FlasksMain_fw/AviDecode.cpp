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

FIL ifile;

#if 1 // ============================ AVI headers ==============================
#define FBUF_SZ     32768 // Packed frame must fit here
#define FOURCC(c1, c2, c3, c4)  ((c4 << 24) | (c3 << 16) | (c2 << 8) | c1)

class ChunkHdr_t {
public:
    uint32_t ckID;
    uint32_t ckSize = 0; //  includes the size of listType plus the size of listData
    uint32_t Type;
    uint8_t ReadNext() {
        if(ckSize & 1) ckSize++; // The data is always padded to nearest WORD boundary. ckSize gives the size of the valid data in the chunk; it does not include the padding
        f_lseek(&ifile, f_tell(&ifile) + ckSize);
        if(f_eof(&ifile)) return retvEndOfFile;
        uint32_t BytesRead = 0;
        if(f_read(&ifile, this, 8, &BytesRead) != FR_OK or BytesRead != 8) return retvFail;
        return retvOk;
    }
    uint8_t ReadType() {
        uint32_t BytesRead = 0;
        if(f_read(&ifile, &Type, 4, &BytesRead) != FR_OK or BytesRead != 4) return retvFail;
        ckSize -= BytesRead;
        return retvOk;
    }

    void Print() { Printf("%4S; Sz=%u\r", &ckID, ckSize); }
    void PrintWType() { Printf("%4S; Sz=%u; %4S\r", &ckID, ckSize, &Type); }
    bool IsRiff() { return ckID == FOURCC('R', 'I', 'F', 'F'); }
    bool IsList() { return ckID == FOURCC('L', 'I', 'S', 'T'); }
    bool TypeIsHDRL() { return Type == FOURCC('h', 'd', 'r', 'l'); }
    bool TypeIsMOVI() { return Type == FOURCC('m', 'o', 'v', 'i'); }
    bool TypeIsAVI()  { return Type == FOURCC('A', 'V', 'I', ' '); }
    bool TypeIsidx1() { return Type == FOURCC('i', 'd', 'x', '1'); }
} ckHdr;

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
} AviMainHdr;

void AnalyzeMovi(uint32_t moviLoc, uint32_t moviSz) {
    ChunkHdr_t Hdr;
    f_lseek(&ifile, moviLoc);
    while(true) {
        if(Hdr.ReadNext() != retvOk) break;
        Hdr.Print();
        if(moviSz <=  Hdr.ckSize) break;
        else moviSz -= Hdr.ckSize;
    }
}

class ckFrame_t {
private:
    uint32_t ckID;
public:
    uint8_t Buf[FBUF_SZ];
    uint32_t NextLoc = 0;
    uint32_t Sz = 0;
    uint8_t ReadNext() {
        f_lseek(&ifile, NextLoc); // Goto next chunk
        while(true) {
            uint32_t BytesRead = 0;
            // Read next header
            if(f_read(&ifile, &ckID, 4, &BytesRead) != FR_OK or BytesRead != 4) return retvFail;
            if(f_read(&ifile, &Sz,   4, &BytesRead) != FR_OK or BytesRead != 4) return retvFail;
//            Printf("%4S; Sz=%u\r", &ckID, Sz);
            if(ckID == FOURCC('0','0','d','c')) { // found
                if(Sz > FBUF_SZ) {
                    Printf("Too large data\r");
                    return retvFail;
                }
                // Read data
                if(f_read(&ifile, Buf, Sz, &BytesRead) == FR_OK and BytesRead == Sz) {
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
} ckFrame;


static struct {
    uint32_t moviLoc = 0;
} idata;
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

uint32_t MCU_TotalNb = 0;
uint32_t MCU_BlockIndex = 0;
sysinterval_t FrameRate = 0;
JPEG_ConfTypeDef JConf;
JPEG_YCbCrToRGB_Convert_Function pConvert_Function;

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
    MCU_TotalNb = 0;
    MCU_BlockIndex = 0;
    Jpeg::PrepareToStart(ckFrame.Buf, (ckFrame.Sz + 3) / 4);
    JMcuBuf.PrepareToFill();
    Jpeg::Start();
}

uint8_t ProcessMcuBuf() {
    if(!pConvert_Function) return retvInProgress;
    uint32_t ConvertedDataCount;
    Dma2d::Suspend();
    // From, To, BlockIndex, FromSz, *ToSz
    MCU_BlockIndex += pConvert_Function((uint8_t*)JMcuBuf.BufToProcess, *Outbuff.CurrBuf, MCU_BlockIndex, JMcuBuf.DataSzToProcess, &ConvertedDataCount);
    Dma2d::Resume();
    if(MCU_BlockIndex == MCU_TotalNb) return retvOk;
    else return retvInProgress;
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
    while(true) {
        VideoMsg_t Msg = MsgQVideo.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case vcmdStart:
                // Try to get next frame
                if(ckFrame.ReadNext() == retvOk) {
                    StartFrameDecoding();
                }
                else MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
                break;

            case vcmdStop:
                Jpeg::Stop();
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
                    Jpeg::GetInfo(&JConf);
                    // Copy RGB decoded Data to the display FrameBuffer
                    uint32_t width_offset = 0;
                    uint32_t xPos = (LCD_WIDTH - JConf.ImageWidth) / 2;
                    uint32_t yPos = (LCD_HEIGHT - JConf.ImageHeight) / 2;

                    if(JConf.ChromaSubsampling == JPEG_420_SUBSAMPLING) {
                        if((JConf.ImageWidth % 16) != 0) width_offset = 16 - (JConf.ImageWidth % 16);
                    }
                    if(JConf.ChromaSubsampling == JPEG_422_SUBSAMPLING) {
                        if((JConf.ImageWidth % 16) != 0) width_offset = 16 - (JConf.ImageWidth % 16);
                    }
                    if(JConf.ChromaSubsampling == JPEG_444_SUBSAMPLING) {
                        if((JConf.ImageWidth % 8) != 0) width_offset = (JConf.ImageWidth % 8);
                    }

                    // Delay before frame show
                    sysinterval_t Elapsed = chVTTimeElapsedSinceX(FrameStart);
                    Printf("e %u\r", Elapsed);
                    if(Elapsed < FrameRate) {
                        chThdSleep(FrameRate - Elapsed);
                    }
                    FrameStart = chVTGetSystemTimeX();
                    Dma2d::CopyBuffer((uint32_t *)(*Outbuff.CurrBuf), (uint32_t *)FrameBuf1, xPos , yPos, JConf.ImageWidth, JConf.ImageHeight, width_offset);
                    Outbuff.Switch();
                    MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
                }
                break;

            default: break;
        } // switch
    } // while true
}

void HAL_JPEG_InfoReadyCallback(JPEG_ConfTypeDef *pInfo) {
    if(pInfo->ChromaSubsampling == JPEG_420_SUBSAMPLING) {
        if((pInfo->ImageWidth  % 16) != 0) pInfo->ImageWidth  += (16 - (pInfo->ImageWidth % 16));
        if((pInfo->ImageHeight % 16) != 0) pInfo->ImageHeight += (16 - (pInfo->ImageHeight % 16));
    }

    if(pInfo->ChromaSubsampling == JPEG_422_SUBSAMPLING) {
        if((pInfo->ImageWidth % 16) != 0) pInfo->ImageWidth  += (16 - (pInfo->ImageWidth % 16));
        if((pInfo->ImageHeight % 8) != 0) pInfo->ImageHeight += (8 - (pInfo->ImageHeight % 8));
    }

    if(pInfo->ChromaSubsampling == JPEG_444_SUBSAMPLING) {
        if((pInfo->ImageWidth % 8)  != 0) pInfo->ImageWidth  += (8 - (pInfo->ImageWidth % 8));
        if((pInfo->ImageHeight % 8) != 0) pInfo->ImageHeight += (8 - (pInfo->ImageHeight % 8));
    }

    if(JPEG_GetDecodeColorConvertFunc(pInfo, &pConvert_Function, &MCU_TotalNb) != retvOk) {
        Printf("JErr\r");
    }
}

#if 1 // ============================= Top Level ===============================
namespace Avi {

void Init() {
    Outbuff.Init();
    JPEG_InitColorTables(); // Init The JPEG Color Look Up Tables used for YCbCr to RGB conversion
    Jpeg::Init(DmaJpegOutCB);

    Dma2d::Init();

    // Create and start thread
    MsgQVideo.Init();
    chThdCreateStatic(waVideoThd, sizeof(waVideoThd), HIGHPRIO, (tfunc_t)VideoThd, NULL);
}

uint8_t Start(const char* FName) {
    uint8_t Rslt = TryOpenFileRead(FName, &ifile);
    if(Rslt != retvOk) return Rslt;
    Rslt = retvFail;
    // Check header
    if(ckHdr.ReadNext() != retvOk) goto End;
    ckHdr.Print();
    if(!ckHdr.IsRiff()) { Printf("BadHdr\r"); goto End; }
    ckHdr.ReadType();
    if(!ckHdr.TypeIsAVI()) { Printf("BadContent\r"); goto End; }
    ckHdr.ckSize = 0; // To read next chunk, not jumping to end of file
    // Analyze: read all chunks and lists
    idata.moviLoc = 0;
    while(true) {
        if(ckHdr.ReadNext() != retvOk) break;
        if(ckHdr.IsList()) {
            ckHdr.ReadType();
            ckHdr.PrintWType();
            // Process hdrl
            if(ckHdr.TypeIsHDRL()) {
                if(AviMainHdr.Read() == retvOk) {
                    FrameRate = TIME_US2I(AviMainHdr.dwMicroSecPerFrame);
                    Printf("FrameRate_st: %u\r", FrameRate);
                    AviMainHdr.Print();
                    ckHdr.ckSize -= sizeof(AviMainHdr);
                    // Read LIST strl
                    ChunkHdr_t Hdr;
                    if(Hdr.ReadNext() == retvOk) {
                        if(Hdr.IsList()) {
                            Hdr.ReadType();
                            Hdr.PrintWType();
                        }
                        else Hdr.Print();
                    }
                }
            }
            // Process movi
            if(ckHdr.TypeIsMOVI()) {
                idata.moviLoc = f_tell(&ifile);
                ckFrame.NextLoc = idata.moviLoc;
                ckFrame.Sz = 0;
                // Everything is prepared to start
                MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStart));
                return retvOk; // movi chunk found
//                AnalyzeMovi(moviLoc, ckHdr.ckSize);
//                f_lseek(&ifile, moviLoc);
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

#if 1 // ============================== IRQ ====================================
extern "C"
void Vector1F0() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
//    PrintfI("JIRQ: %X\r", JPEG->SR);
    uint32_t Flags = JPEG->SR;
    // If header parsed
    if(Flags & JPEG_SR_HPDF) {
        Jpeg::GetInfo(&JConf);
        HAL_JPEG_InfoReadyCallback(&JConf);
        JPEG->CR &= ~JPEG_CR_HPDIE; // Disable hdr IRQ
        JPEG->CFR = JPEG_CFR_CHPDF; // Clear flag
    }

    if(Flags & JPEG_SR_EOCF) {
//        PrintfI("idma: %u\r", dmaStreamGetTransactionSize(PDmaJOut));
        JPEG->CONFR0 = 0; // Stop decoding
        JPEG->CR &= ~(0x7EUL); // Disable all IRQs
        JPEG->CFR = JPEG_CFR_CEOCF | JPEG_CFR_CHPDF; // Clear all flags
        MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdEndDecode));
    }
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}
#endif
