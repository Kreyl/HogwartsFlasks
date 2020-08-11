/*
 * kl_jpeg.cpp
 *
 *  Created on: 9 авг. 2020 г.
 *      Author: layst
 */

#include "kl_jpeg.h"
#include "board.h"
#include "kl_lib.h"
#include "shell.h"
#include "jpeg_utils.h"

Jpeg_t hjpeg;

/* These are the sample quantization tables given in JPEG spec ISO/IEC 10918-1 standard , section K.1. */
static const uint8_t JPEG_LUM_QuantTable[JPEG_QUANT_TABLE_SIZE] =
{
  16,  11,  10,  16,  24,  40,  51,  61,
  12,  12,  14,  19,  26,  58,  60,  55,
  14,  13,  16,  24,  40,  57,  69,  56,
  14,  17,  22,  29,  51,  87,  80,  62,
  18,  22,  37,  56,  68, 109, 103,  77,
  24,  35,  55,  64,  81, 104, 113,  92,
  49,  64,  78,  87, 103, 121, 120, 101,
  72,  92,  95,  98, 112, 100, 103,  99
};
static const uint8_t JPEG_CHROM_QuantTable[JPEG_QUANT_TABLE_SIZE] =
{
  17,  18,  24,  47,  99,  99,  99,  99,
  18,  21,  26,  66,  99,  99,  99,  99,
  24,  26,  56,  99,  99,  99,  99,  99,
  47,  66,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99
};

#if 1 // ================================ Tables ===============================
#define JPEG_AC_HUFF_TABLE_SIZE  ((uint32_t)162) /* Huffman AC table size : 162 codes*/
#define JPEG_DC_HUFF_TABLE_SIZE  ((uint32_t)12)  /* Huffman AC table size : 12 codes*/

/*
 JPEG Huffman Table Structure definition :
 This implementation of Huffman table structure is compliant with ISO/IEC 10918-1 standard , Annex C Huffman Table specification
 */
typedef struct {
  /* These two fields directly represent the contents of a JPEG DHT marker */
  uint8_t Bits[16];        /*!< bits[k] = # of symbols with codes of length k bits, this parameter corresponds to BITS list in the Annex C */
  uint8_t HuffVal[162];    /*!< The symbols, in order of incremented code length, this parameter corresponds to HUFFVAL list in the Annex C */
} JPEG_ACHuffTableTypeDef;

typedef struct {
    /* These two fields directly represent the contents of a JPEG DHT marker */
    uint8_t Bits[16]; /*!< bits[k] = # of symbols with codes of length k bits, this parameter corresponds to BITS list in the Annex C */
    uint8_t HuffVal[12]; /*!< The symbols, in order of incremented code length, this parameter corresponds to HUFFVAL list in the Annex C */
} JPEG_DCHuffTableTypeDef;

typedef struct {
    uint8_t CodeLength[JPEG_AC_HUFF_TABLE_SIZE]; /*!< Code length  */
    uint32_t HuffmanCode[JPEG_AC_HUFF_TABLE_SIZE]; /*!< HuffmanCode */
} JPEG_AC_HuffCodeTableTypeDef;

typedef struct {
  uint8_t CodeLength[JPEG_DC_HUFF_TABLE_SIZE];        /*!< Code length  */
  uint32_t HuffmanCode[JPEG_DC_HUFF_TABLE_SIZE];    /*!< HuffmanCode */
} JPEG_DC_HuffCodeTableTypeDef;

static const JPEG_DCHuffTableTypeDef JPEG_DCLUM_HuffTable =
{
  { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 },   /*Bits*/
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb }           /*HUFFVAL */
};

static const JPEG_DCHuffTableTypeDef JPEG_DCCHROM_HuffTable =
{
  { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },  /*Bits*/
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb }          /*HUFFVAL */
};


static const JPEG_ACHuffTableTypeDef JPEG_ACLUM_HuffTable =
{
  { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d },  /*Bits*/

  {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,     /*HUFFVAL */
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
  }
};

static const JPEG_ACHuffTableTypeDef JPEG_ACCHROM_HuffTable =
{
  { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 },   /*Bits*/

  {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,      /*HUFFVAL */
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
  }
};

static const uint8_t JPEG_ZIGZAG_ORDER[JPEG_QUANT_TABLE_SIZE] =
{
  0,   1,   8,  16,   9,   2,   3,  10,
  17,  24,  32,  25,  18,  11,   4,   5,
  12,  19,  26,  33,  40,  48,  41,  34,
  27,  20,  13,   6,   7,  14,  21,  28,
  35,  42,  49,  56,  57,  50,  43,  36,
  29,  22,  15,  23,  30,  37,  44,  51,
  58,  59,  52,  45,  38,  31,  39,  46,
  53,  60,  61,  54,  47,  55,  62,  63
};
#endif

/**
  * @brief  Configure the JPEG register huffman tables to be included in the JPEG
  *         file header (used for encoding only)
  * @param  hjpeg pointer to a JPEG_HandleTypeDef structure that contains
  *         the configuration information for JPEG module
  * @retval None
  */
static void JPEG_Set_Huff_DHTMem() {
    JPEG_ACHuffTableTypeDef *HuffTableAC0 = (JPEG_ACHuffTableTypeDef*) (uint32_t) &JPEG_ACLUM_HuffTable;
    JPEG_ACHuffTableTypeDef *HuffTableAC1 = (JPEG_ACHuffTableTypeDef*) (uint32_t) &JPEG_ACCHROM_HuffTable;
    JPEG_DCHuffTableTypeDef *HuffTableDC0 = (JPEG_DCHuffTableTypeDef*) (uint32_t) &JPEG_DCLUM_HuffTable;
    JPEG_DCHuffTableTypeDef *HuffTableDC1 = (JPEG_DCHuffTableTypeDef*) (uint32_t) &JPEG_DCCHROM_HuffTable;
    uint32_t value, index;
    __IO uint32_t *address;

    /* DC0 Huffman Table : BITS*/
    /* DC0 BITS is a 16 Bytes table i.e 4x32bits words from DHTMEM base address to DHTMEM + 3*/
    address = (JPEG->DHTMEM + 3);
    index = 16;
    while(index > 3UL) {
        *address = (((uint32_t) HuffTableDC0->Bits[index - 1UL] & 0xFFUL) << 24)
                | (((uint32_t) HuffTableDC0->Bits[index - 2UL] & 0xFFUL) << 16)
                | (((uint32_t) HuffTableDC0->Bits[index - 3UL] & 0xFFUL) << 8)
                | ((uint32_t) HuffTableDC0->Bits[index - 4UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }
    /* DC0 Huffman Table : Val*/
    /* DC0 VALS is a 12 Bytes table i.e 3x32bits words from DHTMEM base address +4 to DHTMEM + 6 */
    address = (JPEG->DHTMEM + 6);
    index = 12;
    while(index > 3UL) {
        *address =
                (((uint32_t) HuffTableDC0->HuffVal[index - 1UL] & 0xFFUL) << 24)
                        | (((uint32_t) HuffTableDC0->HuffVal[index - 2UL] & 0xFFUL) << 16)
                        | (((uint32_t) HuffTableDC0->HuffVal[index - 3UL] & 0xFFUL) << 8)
                        | ((uint32_t) HuffTableDC0->HuffVal[index - 4UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }

    /* AC0 Huffman Table : BITS*/
    /* AC0 BITS is a 16 Bytes table i.e 4x32bits words from DHTMEM base address + 7 to DHTMEM + 10*/
    address = (JPEG->DHTMEM + 10UL);
    index = 16;
    while(index > 3UL) {
        *address = (((uint32_t) HuffTableAC0->Bits[index - 1UL] & 0xFFUL) << 24)
                | (((uint32_t) HuffTableAC0->Bits[index - 2UL] & 0xFFUL) << 16)
                | (((uint32_t) HuffTableAC0->Bits[index - 3UL] & 0xFFUL) << 8)
                | ((uint32_t) HuffTableAC0->Bits[index - 4UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }
    /* AC0 Huffman Table : Val*/
    /* AC0 VALS is a 162 Bytes table i.e 41x32bits words from DHTMEM base address + 11 to DHTMEM + 51 */
    /* only Byte 0 and Byte 1 of the last word (@ DHTMEM + 51) belong to AC0 VALS table */
    address = (JPEG->DHTMEM + 51);
    value = *address & 0xFFFF0000U;
    value = value | (((uint32_t) HuffTableAC0->HuffVal[161] & 0xFFUL) << 8)
            | ((uint32_t) HuffTableAC0->HuffVal[160] & 0xFFUL);
    *address = value;

    /*continue setting 160 AC0 huffman values */
    address--; /* address = hjpeg->Instance->DHTMEM + 50*/
    index = 160;
    while(index > 3UL) {
        *address = (((uint32_t) HuffTableAC0->HuffVal[index - 1UL] & 0xFFUL) << 24)
                 | (((uint32_t) HuffTableAC0->HuffVal[index - 2UL] & 0xFFUL) << 16)
                 | (((uint32_t) HuffTableAC0->HuffVal[index - 3UL] & 0xFFUL) << 8)
                 | ((uint32_t) HuffTableAC0->HuffVal[index - 4UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }

    /* DC1 Huffman Table : BITS*/
    /* DC1 BITS is a 16 Bytes table i.e 4x32bits words from DHTMEM + 51 base address to DHTMEM + 55*/
    /* only Byte 2 and Byte 3 of the first word (@ DHTMEM + 51) belong to DC1 Bits table */
    address = (JPEG->DHTMEM + 51);
    value = *address & 0x0000FFFFU;
    value = value | (((uint32_t) HuffTableDC1->Bits[1] & 0xFFUL) << 24)
            | (((uint32_t) HuffTableDC1->Bits[0] & 0xFFUL) << 16);
    *address = value;

    /* only Byte 0 and Byte 1 of the last word (@ DHTMEM + 55) belong to DC1 Bits table */
    address = (JPEG->DHTMEM + 55);
    value = *address & 0xFFFF0000U;
    value = value | (((uint32_t) HuffTableDC1->Bits[15] & 0xFFUL) << 8)
            | ((uint32_t) HuffTableDC1->Bits[14] & 0xFFUL);
    *address = value;

    /*continue setting 12 DC1 huffman Bits from DHTMEM + 54 down to DHTMEM + 52*/
    address--;
    index = 12;
    while(index > 3UL) {
        *address = (((uint32_t) HuffTableDC1->Bits[index + 1UL] & 0xFFUL) << 24)
                | (((uint32_t) HuffTableDC1->Bits[index] & 0xFFUL) << 16)
                | (((uint32_t) HuffTableDC1->Bits[index - 1UL] & 0xFFUL) << 8)
                | ((uint32_t) HuffTableDC1->Bits[index - 2UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }
    /* DC1 Huffman Table : Val*/
    /* DC1 VALS is a 12 Bytes table i.e 3x32bits words from DHTMEM base address +55 to DHTMEM + 58 */
    /* only Byte 2 and Byte 3 of the first word (@ DHTMEM + 55) belong to DC1 Val table */
    address = (JPEG->DHTMEM + 55);
    value = *address & 0x0000FFFFUL;
    value = value | (((uint32_t) HuffTableDC1->HuffVal[1] & 0xFFUL) << 24)
            | (((uint32_t) HuffTableDC1->HuffVal[0] & 0xFFUL) << 16);
    *address = value;

    /* only Byte 0 and Byte 1 of the last word (@ DHTMEM + 58) belong to DC1 Val table */
    address = (JPEG->DHTMEM + 58);
    value = *address & 0xFFFF0000UL;
    value = value | (((uint32_t) HuffTableDC1->HuffVal[11] & 0xFFUL) << 8)
            | ((uint32_t) HuffTableDC1->HuffVal[10] & 0xFFUL);
    *address = value;

    /*continue setting 8 DC1 huffman val from DHTMEM + 57 down to DHTMEM + 56*/
    address--;
    index = 8;
    while(index > 3UL) {
        *address = (((uint32_t) HuffTableDC1->HuffVal[index + 1UL] & 0xFFUL) << 24)
                 | (((uint32_t) HuffTableDC1->HuffVal[index] & 0xFFUL) << 16)
                 | (((uint32_t) HuffTableDC1->HuffVal[index - 1UL] & 0xFFUL) << 8)
                        | ((uint32_t) HuffTableDC1->HuffVal[index - 2UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }

    /* AC1 Huffman Table : BITS*/
    /* AC1 BITS is a 16 Bytes table i.e 4x32bits words from DHTMEM base address + 58 to DHTMEM + 62*/
    /* only Byte 2 and Byte 3 of the first word (@ DHTMEM + 58) belong to AC1 Bits table */
    address = (JPEG->DHTMEM + 58);
    value = *address & 0x0000FFFFU;
    value = value | (((uint32_t) HuffTableAC1->Bits[1] & 0xFFUL) << 24)
            | (((uint32_t) HuffTableAC1->Bits[0] & 0xFFUL) << 16);
    *address = value;

    /* only Byte 0 and Byte 1 of the last word (@ DHTMEM + 62) belong to Bits Val table */
    address = (JPEG->DHTMEM + 62);
    value = *address & 0xFFFF0000U;
    value = value | (((uint32_t) HuffTableAC1->Bits[15] & 0xFFUL) << 8)
            | ((uint32_t) HuffTableAC1->Bits[14] & 0xFFUL);
    *address = value;

    /*continue setting 12 AC1 huffman Bits from DHTMEM + 61 down to DHTMEM + 59*/
    address--;
    index = 12;
    while(index > 3UL) {
        *address = (((uint32_t) HuffTableAC1->Bits[index + 1UL] & 0xFFUL) << 24)
                | (((uint32_t) HuffTableAC1->Bits[index] & 0xFFUL) << 16)
                | (((uint32_t) HuffTableAC1->Bits[index - 1UL] & 0xFFUL) << 8)
                | ((uint32_t) HuffTableAC1->Bits[index - 2UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }
    /* AC1 Huffman Table : Val*/
    /* AC1 VALS is a 162 Bytes table i.e 41x32bits words from DHTMEM base address + 62 to DHTMEM + 102 */
    /* only Byte 2 and Byte 3 of the first word (@ DHTMEM + 62) belong to AC1 VALS table */
    address = (JPEG->DHTMEM + 62);
    value = *address & 0x0000FFFFUL;
    value = value | (((uint32_t) HuffTableAC1->HuffVal[1] & 0xFFUL) << 24)
            | (((uint32_t) HuffTableAC1->HuffVal[0] & 0xFFUL) << 16);
    *address = value;

    /*continue setting 160 AC1 huffman values from DHTMEM + 63 to DHTMEM+102 */
    address = (JPEG->DHTMEM + 102);
    index = 160;
    while(index > 3UL) {
        *address =
                (((uint32_t) HuffTableAC1->HuffVal[index + 1UL] & 0xFFUL) << 24)
              | (((uint32_t) HuffTableAC1->HuffVal[index] & 0xFFUL) << 16)
              | (((uint32_t) HuffTableAC1->HuffVal[index - 1UL] & 0xFFUL) << 8)
              | ((uint32_t) HuffTableAC1->HuffVal[index - 2UL] & 0xFFUL);
        address--;
        index -= 4UL;
    }
}

/**
  * @brief  Generates Huffman sizes/Codes Table from Bits/vals Table
  * @param  Bits pointer to bits table
  * @param  Huffsize pointer to sizes table
  * @param  Huffcode pointer to codes table
  * @param  LastK pointer to last Coeff (table dimmension)
  * @retval HAL status
  */
static uint8_t JPEG_Bits_To_SizeCodes(uint8_t *Bits, uint8_t *Huffsize, uint32_t *Huffcode, uint32_t *LastK) {
    uint32_t i, p, l, code, si;
    /* Figure C.1: Generation of table of Huffman code sizes */
    p = 0;
    for(l = 0; l < 16UL; l++) {
        i = (uint32_t) Bits[l];
        if((p + i) > 256UL) return retvFail; // check for table overflow
        while(i != 0UL) {
            Huffsize[p] = (uint8_t) l + 1U;
            p++;
            i--;
        }
    }
    Huffsize[p] = 0;
    *LastK = p;

    /* Figure C.2: Generation of table of Huffman codes */
    code = 0;
    si = Huffsize[0];
    p = 0;
    while(Huffsize[p] != 0U) {
        while(((uint32_t) Huffsize[p]) == si) {
            Huffcode[p] = code;
            p++;
            code++;
        }
        /* code must fit in "size" bits (si), no code is allowed to be all ones*/
        if(si > 31UL) return retvFail;
        if(((uint32_t) code) >= (((uint32_t) 1) << si)) return retvFail;
        code <<= 1;
        si++;
    }
    return retvOk;
}

/**
  * @brief  Transform a Bits/Vals AC Huffman table to sizes/Codes huffman Table
  *         that can programmed to the JPEG encoder registers
  * @param  AC_BitsValsTable pointer to AC huffman bits/vals table
  * @param  AC_SizeCodesTable pointer to AC huffman Sizes/Codes table
  * @retval HAL status
  */
static uint8_t JPEG_ACHuff_BitsVals_To_SizeCodes(
        JPEG_ACHuffTableTypeDef *AC_BitsValsTable,
        JPEG_AC_HuffCodeTableTypeDef *AC_SizeCodesTable) {
    uint8_t huffsize[257];
    uint32_t huffcode[257];
    uint32_t k;
    uint32_t l, lsb, msb;
    uint32_t lastK;

    if(JPEG_Bits_To_SizeCodes(AC_BitsValsTable->Bits, huffsize, huffcode, &lastK) != retvOk) return retvFail;

    /* Figure C.3: Ordering procedure for encoding procedure code tables */
    k = 0;
    while(k < lastK) {
        l = AC_BitsValsTable->HuffVal[k];
        if(l == 0UL) {
            l = 160; /*l = 0x00 EOB code*/
        }
        else if(l == 0xF0UL) /* l = 0xF0 ZRL code*/
        {
            l = 161;
        }
        else {
            msb = (l & 0xF0UL) >> 4;
            lsb = (l & 0x0FUL);
            l = (msb * 10UL) + lsb - 1UL;
        }
        if(l >= JPEG_AC_HUFF_TABLE_SIZE) return retvFail; /* Huffman Table overflow error*/
        else {
            AC_SizeCodesTable->HuffmanCode[l] = huffcode[k];
            AC_SizeCodesTable->CodeLength[l] = huffsize[k] - 1U;
            k++;
        }
    }
    return retvOk;
}

/**
  * @brief  Set the JPEG register with an AC huffman table at the given AC table address
  * @param  hjpeg pointer to a JPEG_HandleTypeDef structure that contains
  *         the configuration information for JPEG module
  * @param  HuffTableAC pointer to AC huffman table
  * @param  ACTableAddress Encoder AC huffman table address it could be HUFFENC_AC0 or HUFFENC_AC1.
  * @retval HAL status
  */
static uint8_t JPEG_Set_HuffAC_Mem(JPEG_ACHuffTableTypeDef *HuffTableAC, const __IO uint32_t *ACTableAddress) {
    JPEG_AC_HuffCodeTableTypeDef acSizeCodesTable;
    uint32_t i, lsb, msb;
    __IO uint32_t *address, *addressDef;

    if(ACTableAddress == (JPEG->HUFFENC_AC0)) {
        address = (JPEG->HUFFENC_AC0 + (JPEG_AC_HUFF_TABLE_SIZE / 2UL));
    }
    else if(ACTableAddress == (JPEG->HUFFENC_AC1)) {
        address = (JPEG->HUFFENC_AC1 + (JPEG_AC_HUFF_TABLE_SIZE / 2UL));
    }
    else return retvFail;

    if(HuffTableAC != NULL) {
        if(JPEG_ACHuff_BitsVals_To_SizeCodes(HuffTableAC, &acSizeCodesTable) != retvOk) return retvFail;
        /* Default values settings: 162:167 FFFh , 168:175 FD0h_FD7h */
        /* Locations 162:175 of each AC table contain information used internally by the core */

        addressDef = address;
        for(i = 0; i < 3UL; i++) {
            *addressDef = 0x0FFF0FFF;
            addressDef++;
        }
        *addressDef = 0x0FD10FD0;
        addressDef++;
        *addressDef = 0x0FD30FD2;
        addressDef++;
        *addressDef = 0x0FD50FD4;
        addressDef++;
        *addressDef = 0x0FD70FD6;
        /* end of Locations 162:175  */

        i = JPEG_AC_HUFF_TABLE_SIZE;
        while(i > 1UL) {
            i--;
            address--;
            msb = ((uint32_t) (((uint32_t) acSizeCodesTable.CodeLength[i] & 0xFU) << 8))
                            | ((uint32_t) acSizeCodesTable.HuffmanCode[i] & 0xFFUL);
            i--;
            lsb = ((uint32_t) (((uint32_t) acSizeCodesTable.CodeLength[i] & 0xFU) << 8))
                            | ((uint32_t) acSizeCodesTable.HuffmanCode[i] & 0xFFUL);
            *address = lsb | (msb << 16);
        }
    }
    return retvOk;
}

/**
  * @brief  Transform a Bits/Vals DC Huffman table to sizes/Codes huffman Table
  *         that can programmed to the JPEG encoder registers
  * @param  DC_BitsValsTable pointer to DC huffman bits/vals table
  * @param  DC_SizeCodesTable pointer to DC huffman Sizes/Codes table
  * @retval HAL status
  */
static uint8_t JPEG_DCHuff_BitsVals_To_SizeCodes(JPEG_DCHuffTableTypeDef *DC_BitsValsTable, JPEG_DC_HuffCodeTableTypeDef *DC_SizeCodesTable) {
    uint32_t k;
    uint32_t l;
    uint32_t lastK;
    uint8_t huffsize[257];
    uint32_t huffcode[257];
    if(JPEG_Bits_To_SizeCodes(DC_BitsValsTable->Bits, huffsize, huffcode, &lastK) != retvOk) return retvFail;
    /* Figure C.3: ordering procedure for encoding procedure code tables */
    k = 0;
    while(k < lastK) {
        l = DC_BitsValsTable->HuffVal[k];
        if(l >= JPEG_DC_HUFF_TABLE_SIZE) return retvFail; /* Huffman Table overflow error*/
        else {
            DC_SizeCodesTable->HuffmanCode[l] = huffcode[k];
            DC_SizeCodesTable->CodeLength[l] = huffsize[k] - 1U;
            k++;
        }
    }
    return retvOk;
}

/**
  * @brief  Set the JPEG register with an DC huffman table at the given DC table address
  * @param  hjpeg pointer to a JPEG_HandleTypeDef structure that contains
  *         the configuration information for JPEG module
  * @param  HuffTableDC pointer to DC huffman table
  * @param  DCTableAddress Encoder DC huffman table address it could be HUFFENC_DC0 or HUFFENC_DC1.
  * @retval HAL status
  */
static uint8_t JPEG_Set_HuffDC_Mem(JPEG_DCHuffTableTypeDef *HuffTableDC, const __IO uint32_t *DCTableAddress) {
    JPEG_DC_HuffCodeTableTypeDef dcSizeCodesTable;
    uint32_t i, lsb, msb;
    __IO uint32_t *address, *addressDef;

    if(DCTableAddress == (JPEG->HUFFENC_DC0)) {
        address = (JPEG->HUFFENC_DC0 + (JPEG_DC_HUFF_TABLE_SIZE / 2UL));
    }
    else if(DCTableAddress == (JPEG->HUFFENC_DC1)) {
        address = (JPEG->HUFFENC_DC1 + (JPEG_DC_HUFF_TABLE_SIZE / 2UL));
    }
    else return retvFail;

    if(HuffTableDC != NULL) {
        if(JPEG_DCHuff_BitsVals_To_SizeCodes(HuffTableDC, &dcSizeCodesTable) != retvOk) return retvFail;

        addressDef = address;
        *addressDef = 0x0FFF0FFF;
        addressDef++;
        *addressDef = 0x0FFF0FFF;

        i = JPEG_DC_HUFF_TABLE_SIZE;
        while(i > 1UL) {
            i--;
            address--;
            msb = ((uint32_t) (((uint32_t) dcSizeCodesTable.CodeLength[i] & 0xFU) << 8))
                            | ((uint32_t) dcSizeCodesTable.HuffmanCode[i] & 0xFFUL);
            i--;
            lsb = ((uint32_t) (((uint32_t) dcSizeCodesTable.CodeLength[i] & 0xFU) << 8))
                            | ((uint32_t) dcSizeCodesTable.HuffmanCode[i] & 0xFFUL);
            *address = lsb | (msb << 16);
        }
    }
    return retvOk;
}

/**
  * @brief  Configure the JPEG encoder register huffman tables to used during
  *         the encdoing operation
  * @param  hjpeg pointer to a JPEG_HandleTypeDef structure that contains
  *         the configuration information for JPEG module
  * @retval None
  */
static uint8_t JPEG_Set_HuffEnc_Mem() {
    JPEG_Set_Huff_DHTMem();
    if(JPEG_Set_HuffAC_Mem((JPEG_ACHuffTableTypeDef*)&JPEG_ACLUM_HuffTable,   JPEG->HUFFENC_AC0) != retvOk) return retvFail;
    if(JPEG_Set_HuffAC_Mem((JPEG_ACHuffTableTypeDef*)&JPEG_ACCHROM_HuffTable, JPEG->HUFFENC_AC1) != retvOk) return retvFail;
    if(JPEG_Set_HuffDC_Mem((JPEG_DCHuffTableTypeDef*)&JPEG_DCLUM_HuffTable,   JPEG->HUFFENC_DC0) != retvOk) return retvFail;
    if(JPEG_Set_HuffDC_Mem((JPEG_DCHuffTableTypeDef*)&JPEG_DCCHROM_HuffTable, JPEG->HUFFENC_DC1) != retvOk) return retvFail;
    return retvOk;
}


void InitJPEG() {
    rccEnableAHB2(RCC_AHB2ENR_JPEGEN, FALSE);

    /* Change the JPEG state */
//    hjpeg.State = HAL_JPEG_STATE_BUSY;
    JPEG->CR = JPEG_CR_JCEN; // En and clear all the other
    JPEG->CONFR0 = 0; /* Stop the JPEG encoding/decoding process*/
    JPEG->CR = JPEG_CR_JCEN; // En and clear all the other

    /* Flush input and output FIFOs*/
    JPEG->CR |= JPEG_CR_IFF;
    JPEG->CR |= JPEG_CR_OFF;

    JPEG->CFR = JPEG_CFR_CEOCF | JPEG_CFR_CHPDF; /* Clear all flags */

    /* init default quantization tables*/
    hjpeg.QuantTable0 = (uint8_t *)((uint32_t)JPEG_LUM_QuantTable);
    hjpeg.QuantTable1 = (uint8_t *)((uint32_t)JPEG_CHROM_QuantTable);
    hjpeg.QuantTable2 = NULL;
    hjpeg.QuantTable3 = NULL;

    /* init the default Huffman tables*/
    if(JPEG_Set_HuffEnc_Mem() != retvOk) {
        Printf("Jpg Init Fail\r");
        return;
    }

    /* Enable header processing*/
    JPEG->CONFR1 |= JPEG_CONFR1_HDR;

    /* Reset JpegInCount and JpegOutCount */
    hjpeg.JpegInCount = 0;
    hjpeg.JpegOutCount = 0;

    /* Change the JPEG state */
//    hjpeg.State = HAL_JPEG_STATE_READY;

    /* Reset the JPEG ErrorCode */
//    hjpeg->ErrorCode = HAL_JPEG_ERROR_NONE;

    hjpeg.Context = 0; /*Clear the context filelds*/
    Printf("Jpg Init ok\r");
}

__unused
static uint32_t JPEG_GetQuality() {
    uint32_t quality = 0;
    uint32_t quantRow, quantVal, scale, i, j;
    __IO uint32_t *tableAddress = JPEG->QMEM0;
    i = 0;
    while(i < (JPEG_QUANT_TABLE_SIZE - 3UL)) {
        quantRow = *tableAddress;
        for(j = 0; j < 4UL; j++) {
            quantVal = (quantRow >> (8UL * j)) & 0xFFUL;
            if(quantVal == 1UL) {
                /* if Quantization value = 1 then quality is 100%*/
                quality += 100UL;
            }
            else {
                /* Note that the quantization coefficients must be specified in the table in zigzag order */
                scale = (quantVal * 100UL) / ((uint32_t) hjpeg.QuantTable0[JPEG_ZIGZAG_ORDER[i + j]]);
                if(scale <= 100UL) {
                    quality += (200UL - scale) / 2UL;
                }
                else {
                    quality += 5000UL / scale;
                }
            }
        }

        i += 4UL;
        tableAddress++;
    }
    return (quality / 64UL);
}
