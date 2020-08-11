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
#include "color.h"

FIL ifile;

#if 1 // ============================ AVI headers ==============================
#define FBUF_SZ     24000
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
            Printf("%4S; Sz=%u\r", &ckID, Sz);
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
enum VideoCmd_t { vcmdNone, vcmdStart, vcmdStop, vcmdNewBufReady };

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

#if 1 // ============================= Decoding ================================
static const stm32_dma_stream_t *PDmaJIn;
static const stm32_dma_stream_t *PDmaJOut;

#define JPEG_DMA_IN_MODE  (STM32_DMA_CR_CHSEL(JPEG_DMA_CHNL) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_WORD | STM32_DMA_CR_PSIZE_WORD | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define JPEG_DMA_OUT_MODE (STM32_DMA_CR_CHSEL(JPEG_DMA_CHNL) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_WORD | STM32_DMA_CR_PSIZE_WORD | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_TCIE)

#define OUTBUF_SZ   768000UL
#define MCU_BUF_CNT (768UL / 4)
#define MCU_BUF_SZ  (MCU_BUF_CNT * sizeof(uint32_t))

uint32_t MCU_TotalNb = 0;
uint32_t MCU_BlockIndex = 0;
JPEG_YCbCrToRGB_Convert_Function pConvert_Function;

uint8_t *OutBuf = nullptr;

//extern ColorARGB_t *FrameBuf1;

class {
private:
    uint32_t Buf1[MCU_BUF_CNT];
    uint32_t Buf2[MCU_BUF_CNT];
    uint32_t *BufToFill = Buf1;
public:
    uint32_t *BufToProcess = Buf2, DataSzToProcess = 0;

    void PrepareToFill() {
        dmaStreamSetMemory0(PDmaJOut, BufToFill);
        dmaStreamSetTransactionSize(PDmaJOut, MCU_BUF_CNT);
        dmaStreamSetMode(PDmaJOut, JPEG_DMA_OUT_MODE);
        dmaStreamEnable(PDmaJOut);
    }

    void Switch() {
        dmaStreamDisable(PDmaJOut);
        DataSzToProcess = (MCU_BUF_CNT - dmaStreamGetTransactionSize(PDmaJOut)) * sizeof(uint32_t);
        if(BufToFill == Buf1) {
            BufToFill = Buf2;
            BufToProcess = Buf1;
        }
        else {
            BufToFill = Buf1;
            BufToProcess = Buf2;
        }
        PrepareToFill();
    }
} JMcuBuf;

void JpegStop() {
    JPEG->CONFR0 = 0; // Stop decoding
    JPEG->CR &= ~(JPEG_CR_IDMAEN | JPEG_CR_ODMAEN);
    dmaStreamDisable(PDmaJIn);
    dmaStreamDisable(PDmaJOut);
}

void StartFrameDecoding() {
    JpegStop();
    MCU_TotalNb = 0;
    MCU_BlockIndex = 0;
    // Flush input and output FIFOs
    JPEG->CR |= JPEG_CR_IFF;
    JPEG->CR |= JPEG_CR_OFF;
    // Prepare JPEG
    JPEG->CFR = JPEG_CFR_CEOCF | JPEG_CFR_CHPDF; // Clear all flags
    JPEG->CR |= JPEG_CR_EOCIE | JPEG_CR_HPDIE; // IRQ on EndOfConversion and EndOfHdrParsing
    JPEG->CR |= (JPEG_CR_IDMAEN | JPEG_CR_ODMAEN); // Enable DMAs

    // Prepare DMA
    dmaStreamSetMemory0(PDmaJIn, ckFrame.Buf);
    dmaStreamSetTransactionSize(PDmaJIn, (ckFrame.Sz + 3) / 4);
    dmaStreamSetMode(PDmaJIn, JPEG_DMA_IN_MODE);
    dmaStreamEnable(PDmaJIn);

    JMcuBuf.PrepareToFill();

    nvicEnableVector(JPEG_IRQn, IRQ_PRIO_MEDIUM);

    JPEG->CONFR0 |=  JPEG_CONFR0_START; // Start everything
}

void ProcessMcuBuf() {
    uint32_t ConvertedDataCount;
    // From, To, BlockIndex, FromSz, *ToSz
    MCU_BlockIndex += pConvert_Function((uint8_t*)JMcuBuf.BufToProcess, OutBuf, MCU_BlockIndex, JMcuBuf.DataSzToProcess, &ConvertedDataCount);
    Printf(" bi: %u\r", MCU_BlockIndex);
    if(MCU_BlockIndex == MCU_TotalNb) Printf("Aga!\r");
}

#endif


void DmaJpegInCB(void *p, uint32_t flags) {
    PrintfI("InIRQ\r");
}

void DmaJpegOutCB(void *p, uint32_t flags) {
    PrintfI("OutIRQ\r");
    chSysLockFromISR();
    MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdNewBufReady));
    chSysUnlockFromISR();
}

static THD_WORKING_AREA(waVideoThd, 1024);
__noreturn
static void VideoThd(void *arg) {
    chRegSetThreadName("Video");
    while(true) {
        VideoMsg_t Msg = MsgQVideo.Fetch(TIME_INFINITE);
        switch(Msg.Cmd) {
            case vcmdStart:
                // Try to get next frame
                if(ckFrame.ReadNext() == retvOk) StartFrameDecoding();
                else MsgQVideo.SendNowOrExit(VideoMsg_t(vcmdStop));
                break;

            case vcmdStop:
                JpegStop();
                CloseFile(&ifile);
                break;

            case vcmdNewBufReady:
                JMcuBuf.Switch(); // Switch buf and continue reception
                ProcessMcuBuf();  // Process what received
                break;

            default: break;
        } // switch
    } // while true
}

static void HAL_JPEG_GetInfo() {
    JPEG_ConfTypeDef *pInfo = &hjpeg.Conf;
    // Colorspace
    if     ((JPEG->CONFR1 & JPEG_CONFR1_NF) == JPEG_CONFR1_NF_1) pInfo->ColorSpace = JPEG_YCBCR_COLORSPACE;
    else if((JPEG->CONFR1 & JPEG_CONFR1_NF) == 0UL)              pInfo->ColorSpace = JPEG_GRAYSCALE_COLORSPACE;
    else if((JPEG->CONFR1 & JPEG_CONFR1_NF) == JPEG_CONFR1_NF)   pInfo->ColorSpace = JPEG_CMYK_COLORSPACE;
    // x y
    pInfo->ImageHeight = (JPEG->CONFR1 & 0xFFFF0000UL) >> 16;
    pInfo->ImageWidth  = (JPEG->CONFR3 & 0xFFFF0000UL) >> 16;

    uint32_t yblockNb, cBblockNb, cRblockNb;
    if((pInfo->ColorSpace == JPEG_YCBCR_COLORSPACE) or (pInfo->ColorSpace == JPEG_CMYK_COLORSPACE)) {
        yblockNb  = (JPEG->CONFR4 & JPEG_CONFR4_NB) >> 4;
        cBblockNb = (JPEG->CONFR5 & JPEG_CONFR5_NB) >> 4;
        cRblockNb = (JPEG->CONFR6 & JPEG_CONFR6_NB) >> 4;

        if((yblockNb == 1UL) && (cBblockNb == 0UL) && (cRblockNb == 0UL)) {
            pInfo->ChromaSubsampling = JPEG_422_SUBSAMPLING; /*16x8 block*/
        }
        else if((yblockNb == 0UL) && (cBblockNb == 0UL) && (cRblockNb == 0UL)) {
            pInfo->ChromaSubsampling = JPEG_444_SUBSAMPLING;
        }
        else if((yblockNb == 3UL) && (cBblockNb == 0UL) && (cRblockNb == 0UL)) {
            pInfo->ChromaSubsampling = JPEG_420_SUBSAMPLING;
        }
        else /*Default is 4:4:4*/
        {
            pInfo->ChromaSubsampling = JPEG_444_SUBSAMPLING;
        }
    }
    else {
        pInfo->ChromaSubsampling = JPEG_444_SUBSAMPLING;
    }
    /* Note : the image quality is only available at the end of the decoding operation */
     /* at the current stage the calculated image quality is not correct so reset it */
    pInfo->ImageQuality = 0;
}

void HAL_JPEG_InfoReadyCallback() {
    JPEG_ConfTypeDef *pInfo = &hjpeg.Conf;
    if(pInfo->ChromaSubsampling == JPEG_420_SUBSAMPLING) {
        if((pInfo->ImageWidth % 16) != 0)
            pInfo->ImageWidth += (16 - (pInfo->ImageWidth % 16));

        if((pInfo->ImageHeight % 16) != 0)
            pInfo->ImageHeight += (16 - (pInfo->ImageHeight % 16));
    }

    if(pInfo->ChromaSubsampling == JPEG_422_SUBSAMPLING) {
        if((pInfo->ImageWidth % 16) != 0)
            pInfo->ImageWidth += (16 - (pInfo->ImageWidth % 16));

        if((pInfo->ImageHeight % 8) != 0)
            pInfo->ImageHeight += (8 - (pInfo->ImageHeight % 8));
    }

    if(pInfo->ChromaSubsampling == JPEG_444_SUBSAMPLING) {
        if((pInfo->ImageWidth % 8) != 0)
            pInfo->ImageWidth += (8 - (pInfo->ImageWidth % 8));

        if((pInfo->ImageHeight % 8) != 0)
            pInfo->ImageHeight += (8 - (pInfo->ImageHeight % 8));
    }

    if(JPEG_GetDecodeColorConvertFunc(pInfo, &pConvert_Function, &MCU_TotalNb) != retvOk) {
        Printf("JErr\r");
    }
}

#if 1 // ============================= Top Level ===============================
namespace Avi {

void Init() {
    OutBuf = (uint8_t*)malloc(OUTBUF_SZ);
    JPEG_InitColorTables(); // Init The JPEG Color Look Up Tables used for YCbCr to RGB conversion
    InitJPEG();
    JPEG->CONFR1 |= JPEG_CONFR1_DE | JPEG_CONFR1_HDR; // En Decode and Hdr processing

    // DMA
    PDmaJIn = dmaStreamAlloc(JPEG_DMA_IN, IRQ_PRIO_MEDIUM, DmaJpegInCB, nullptr);
    dmaStreamSetPeripheral(PDmaJIn, &JPEG->DIR);
    PDmaJOut = dmaStreamAlloc(JPEG_DMA_OUT, IRQ_PRIO_MEDIUM, DmaJpegOutCB, nullptr);
    dmaStreamSetPeripheral(PDmaJOut, &JPEG->DOR);
    // Create and start thread
    MsgQVideo.Init();
    chThdCreateStatic(waVideoThd, sizeof(waVideoThd), NORMALPRIO, (tfunc_t)VideoThd, NULL);
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
                    AviMainHdr.Print();
                    ckHdr.ckSize -= sizeof(AviMainHdr);
                    // Read LIST strl
//                    ChunkHdr_t Hdr;
//                    if(Hdr.ReadNext() == retvOk) {
//                        if(Hdr.IsList()) {
//                            Hdr.ReadType();
//                            Hdr.PrintWType();
//                        }
//                        else Hdr.Print();
//                    }
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
    PrintfI("JIRQ: %X\r", JPEG->SR);
    // If header parsed
    if(JPEG->SR & JPEG_SR_HPDF) {
        HAL_JPEG_GetInfo();
        HAL_JPEG_InfoReadyCallback();
        JPEG->CR &= ~JPEG_CR_HPDIE; // Disable hdr IRQ
        JPEG->CFR = JPEG_CFR_CHPDF; // Clear flag
    }

    if(JPEG->SR & JPEG_SR_EOCF) {
        JpegStop(); // Stop decoding, disable DMA
        JPEG->CR &= ~(0x7EUL); // Disable all IRQs
        JPEG->CFR = JPEG_CFR_CEOCF | JPEG_CFR_CHPDF; // Clear all flags
        MsgQVideo.SendNowOrExitI(VideoMsg_t(vcmdNewBufReady));
    }
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}
#endif
