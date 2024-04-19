/*
 * sdram.h
 *
 *  Created on: 4 ����. 2019 �.
 *      Author: Kreyl
 */

#ifndef SDRAM_H__
#define SDRAM_H__

#include <inttypes.h>

#define REMAP_TO_0x6ETC

#define SDRAM_SZ            33554432    // 32MBytes
#define SDRAM_SZ_DWORDS     (SDRAM_SZ / 4)
#ifdef REMAP_TO_0x6ETC
#define SDRAM_ADDR          0x60000000
#else
#define SDRAM_ADDR          0xC0000000
#endif

void SdramInit();
void SdramDeinit();
void SdramCheck();

/* else if(PCmd->NameIs("chk")) { SdramCheck(); }  */

#endif //SDRAM_H__