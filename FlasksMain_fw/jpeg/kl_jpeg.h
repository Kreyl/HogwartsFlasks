/*
 * kl_jpeg.h
 *
 *  Created on: 9 авг. 2020 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>

/** @defgroup JPEG_Configuration_Structure_definition JPEG Configuration for encoding Structure definition
  * @brief  JPEG encoding configuration Structure definition
  * @{
  */
typedef struct {
  uint32_t ColorSpace;               /*!< Image Color space : gray-scale, YCBCR, RGB or CMYK
                                           This parameter can be a value of @ref JPEG_ColorSpace */
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

class Jpeg_t {
public:
    JPEG_ConfTypeDef         Conf;             /*!< Current JPEG encoding/decoding parameters */
    uint8_t                  *pJpegInBuffPtr;  /*!< Pointer to JPEG processing (encoding, decoding,...) input buffer */
    uint8_t                  *pJpegOutBuffPtr; /*!< Pointer to JPEG processing (encoding, decoding,...) output buffer */
    volatile uint32_t        JpegInCount;      /*!< Internal Counter of input data */
    volatile uint32_t        JpegOutCount;     /*!< Internal Counter of output data */
    uint32_t                 InDataLength;     /*!< Input Buffer Length in Bytes */
    uint32_t                 OutDataLength;    /*!< Output Buffer Length in Bytes */
//    DMA_HandleTypeDef        *hdmain;          /*!< JPEG In DMA handle parameters */
//    DMA_HandleTypeDef        *hdmaout;         /*!< JPEG Out DMA handle parameters */
    uint8_t                  CustomQuanTable;  /*!< If set to 1 specify that user customized quantization tables are used */
    uint8_t                  *QuantTable0;     /*!< Basic Quantization Table for component 0 */
    uint8_t                  *QuantTable1;     /*!< Basic Quantization Table for component 1 */
    uint8_t                  *QuantTable2;     /*!< Basic Quantization Table for component 2 */
    uint8_t                  *QuantTable3;     /*!< Basic Quantization Table for component 3 */

    bool IsLocked = false;             /*!< JPEG locking object */

//    volatile HAL_JPEG_STATETypeDef State = HAL_JPEG_STATE_RESET;         /*!< JPEG peripheral state */
//    volatile uint32_t           ErrorCode;        /*!< JPEG Error code */
    volatile uint32_t Context;                     /*!< JPEG Internal context */

};

extern Jpeg_t hjpeg;

void InitJPEG();


