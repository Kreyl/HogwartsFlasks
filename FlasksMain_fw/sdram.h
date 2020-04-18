/*
 * sdram.h
 *
 *  Created on: 4 но€б. 2019 г.
 *      Author: Kreyl
 */

#pragma once

#include <inttypes.h>

#define SDRAM_SZ            33554432    // 32MBytes
#define SDRAM_SZ_DWORDS     (SDRAM_SZ / 4)
#define SDRAM_ADDR          0xC0000000

void SdramInit();
void SdramCheck();

/* else if(PCmd->NameIs("chk")) { SdramCheck(); }  */
