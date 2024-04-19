#ifndef WS2812B_H__
#define WS2812B_H__

/*
 * ========== WS2812 control module ==========
 * Only basic command "SetCurrentColors" is implemented, all other is up to
 * higher level software.
 * WS2812 requires following timings, ns: (400 + 850) +- 150 each.
 *
 * SPI bitrate must be 2500kHz => 400nS per bit.
 * Then, LED's 0 is 100 (400 + 800), LED's 1 is 110 (800 + 400).
 * Reset must be:
 *    WS2812: 50uS => 125bit => ~16 bytes
 *    WS2813: 300uS => 750bit => 94 bytes
 */

#include "board.h"
#include "ch.h"
#include "hal.h"
#include "kl_lib.h"
#include "color.h"

class Neopixels_t {
private:
    const uint32_t LedCnt;
    uint8_t *BitBuf;
    uint32_t BitBufSz;
    const Spi_t ISpi;
    const GPIO_TypeDef *PGpio;
    const uint16_t Pin;
    const AlterFunc_t Af;
    const uint32_t DmaID;
    const uint32_t DmaMode;
    const stm32_dma_stream_t *PDma = nullptr;
public:
    bool TransmitDone = false;
    ftVoidVoid OnTransmitEnd = nullptr;
    Color_t *ClrBuf;
    Neopixels_t(uint32_t ALedCnt, uint8_t *PBitBuf,
            SPI_TypeDef *ASpi, GPIO_TypeDef *APGpio, uint16_t APin, AlterFunc_t AAf,
            uint32_t ADmaID, uint32_t ADmaMode, Color_t *AClrBuf);
    void Init();
    void SetCurrentColors();
    void SetAll(Color_t Clr);
    bool AreOff();
    void OnDmaDoneI();
};

extern Neopixels_t Npx;

#if NPX2_LED_CNT
extern Neopixels_t Npx2;
#endif

#endif //WS2812B_H__