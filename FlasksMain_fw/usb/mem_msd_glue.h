/*
 * mem_msd_glue.h
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#pragma once

#include "kl_sd.h"
#include "board.h"

extern SDCDriver SDC_DRV;

#define MSD_BLOCK_CNT   SDC_DRV.capacity
#define MSD_BLOCK_SZ    512

uint8_t MSDRead(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt);
uint8_t MSDWrite(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt);
