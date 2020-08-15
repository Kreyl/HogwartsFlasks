/*
 * kl_jpeg.h
 *
 *  Created on: 9 авг. 2020 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>
#include "ch.h"
#include "stm32_dma.h"

/** @defgroup JPEG_Configuration_Structure_definition JPEG Configuration for encoding Structure definition
  * @brief  JPEG encoding configuration Structure definition
  * @{
  */
typedef struct {
  uint32_t ColorSpace;               /*!< Image Color space : gray-scale, YCBCR, RGB or CMYK. This parameter can be a value of @ref JPEG_ColorSpace */
  uint32_t ChromaSubsampling;        /*!< Chroma Subsampling in case of YCBCR or CMYK color space, 0-> 4:4:4 , 1-> 4:2:2, 2 -> 4:1:1, 3 -> 4:2:0
                                           This parameter can be a value of @ref JPEG_ChromaSubsampling */
  uint32_t ImageHeight;              /*!< Image height : number of lines */
  uint32_t ImageWidth;               /*!< Image width : number of pixels per line */
  uint32_t ImageQuality;             /*!< Quality of the JPEG encoding : from 1 to 100 */
} JPEG_ConfTypeDef;

/** @defgroup JPEG_ColorSpace JPEG ColorSpace
  * @brief  JPEG Color Space
  * @{
  */
#define JPEG_GRAYSCALE_COLORSPACE     ((uint32_t)0x00000000U)
#define JPEG_YCBCR_COLORSPACE         JPEG_CONFR1_COLORSPACE_0
#define JPEG_CMYK_COLORSPACE          JPEG_CONFR1_COLORSPACE

/** @defgroup JPEG_ChromaSubsampling JPEG Chrominance Sampling
  * @brief  JPEG Chrominance Sampling
  * @{
  */
#define JPEG_444_SUBSAMPLING     ((uint32_t)0x00000000U)   /*!< Chroma Subsampling 4:4:4 */
#define JPEG_420_SUBSAMPLING     ((uint32_t)0x00000001U)   /*!< Chroma Subsampling 4:2:0 */
#define JPEG_422_SUBSAMPLING     ((uint32_t)0x00000002U)   /*!< Chroma Subsampling 4:2:2 */

/** @defgroup JPEG_Quantization_Table_Size JPEG Quantization Table Size
  * @brief  JPEG Quantization Table Size
  * @{
  */
#define JPEG_QUANT_TABLE_SIZE  ((uint32_t)64U) /*!< JPEG Quantization Table Size in bytes  */

namespace Jpeg {

void Init(stm32_dmaisr_t DmaOutCallback);
void GetInfo(JPEG_ConfTypeDef *pInfo);

void PrepareToStart(void *ptr, uint32_t Cnt);
void DisableOutDma();
uint32_t GetOutDmaTransCnt();
void EnableOutDma(void *ptr, uint32_t Cnt);

void Start();
void Stop();

};
