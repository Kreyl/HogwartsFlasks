/*
 * Windows Bitmap File Loader
 * Version 1.2.5 (20120929)
 *
 * Supported Formats: 1, 4, 8, 16, 24, 32 Bit Images
 * Alpha Bitmaps are also supported.
 * Supported compression types: RLE 8, BITFIELDS
 *
 * Created by: Benjamin Kalytta, 2006 - 2012
 * Thanks for bug fixes goes to: Chris Campbell
 *
 * Licence: Free to use, URL to my source and my name is required in your source code.
 *
 * Source can be found at http://www.kalytta.com/bitmap.h
 *
 * Warning: This code should not be used in unmodified form in a production environment.
 * It should only serve as a basis for your own development.
 * There is only a minimal error handling in this code. (Notice added 20111211)
 */

#pragma once

#include <string>
#include <stdint.h>

#define BITMAP_SIGNATURE 0x4d42

struct BmpFileHeader_t {
	uint16_t Signature;
	uint32_t Size;
	uint32_t Reserved;
	uint32_t BitsOffset;
} __attribute__((packed));

struct BmpHeader_t {
	uint32_t HeaderSize;
	int32_t Width;
	int32_t Height;
	uint16_t Planes;
	uint16_t BitCount;
	uint32_t Compression;
	uint32_t SizeImage;
	int32_t PelsPerMeterX;
	int32_t PelsPerMeterY;
	uint32_t ClrUsed;
	uint32_t ClrImportant;
	uint32_t RedMask;
	uint32_t GreenMask;
	uint32_t BlueMask;
	uint32_t AlphaMask;
	uint32_t CsType;
	uint32_t Endpoints[9]; // see http://msdn2.microsoft.com/en-us/library/ms536569.aspx
	uint32_t GammaRed;
	uint32_t GammaGreen;
	uint32_t GammaBlue;
} __attribute__((packed));

struct RGBA_t {
	uint8_t Red;
	uint8_t Green;
	uint8_t Blue;
	uint8_t Alpha;
} __attribute__((packed));

struct BGRA_t {
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alpha;
} __attribute__((packed));

static uint8_t Line[2048];
static BGRA_t ColorTable[256];

class CBitmap {
private:
    BmpFileHeader_t m_BitmapFileHeader;
    BmpHeader_t m_BitmapHeader;
	uint32_t m_BitmapSize;
public:
	class CColor {
public:
		static inline unsigned int BitCountByMask(unsigned int Mask) {
			unsigned int BitCount = 0;
			while (Mask) {
				Mask &= Mask - 1;
				BitCount++;
			}
			return BitCount;
		}

		static inline unsigned int BitPositionByMask(unsigned int Mask) {
			return BitCountByMask((Mask & (~Mask + 1)) - 1);
		}

		static inline unsigned int ComponentByMask(unsigned int Color, unsigned int Mask) {
			unsigned int Component = Color & Mask;
			return Component >> BitPositionByMask(Mask);
		}

		static inline unsigned int BitCountToMask(unsigned int BitCount) {
			return (BitCount == 32) ? 0xFFFFFFFF : (1 << BitCount) - 1;
		}

		static unsigned int Convert(unsigned int Color, unsigned int FromBitCount, unsigned int ToBitCount) {
			if (ToBitCount < FromBitCount) {
				Color >>= (FromBitCount - ToBitCount);
			} else {
				Color <<= (ToBitCount - FromBitCount);
				if (Color > 0) {
					Color |= BitCountToMask(ToBitCount - FromBitCount);
				}
			}
			return Color;
		}
	};

public:
	RGBA_t *m_BitmapData;

	CBitmap() : m_BitmapSize(0), m_BitmapData(0)  {
		Dispose();
	}

	~CBitmap() {
		Dispose();
	}

	void Dispose() {
		if(m_BitmapData) {
			delete[] m_BitmapData;
			m_BitmapData = 0;
		}
	}

	bool Load(uint8_t *Buff, int32_t Sz) {
		Dispose();
		uint8_t *Ptr = Buff;
		memcpy(&m_BitmapFileHeader, Ptr, sizeof(BmpFileHeader_t));
		if (m_BitmapFileHeader.Signature != BITMAP_SIGNATURE) {
			return false;
		}
		Ptr += sizeof(BmpFileHeader_t);

		memcpy(&m_BitmapHeader, Ptr, sizeof(BmpHeader_t));

		// Load Color Table
		if(m_BitmapHeader.BitCount) {
		    Ptr = Buff + sizeof(BmpFileHeader_t) + m_BitmapHeader.HeaderSize;
		    memcpy(ColorTable, Ptr, sizeof(BGRA_t) * m_BitmapHeader.ClrUsed);
		}

		m_BitmapSize = GetWidth() * GetHeight();
		m_BitmapData = new RGBA_t[m_BitmapSize];

		uint32_t LineWidth = ((GetWidth() * GetBitCount() / 8) + 3) & ~3;

		Ptr = Buff + m_BitmapFileHeader.BitsOffset;

		int Index = 0;
		bool Result = true;

		if (m_BitmapHeader.Compression == 0) {
			for (unsigned int i = 0; i < GetHeight(); i++) {
			    memcpy(Line, Ptr, LineWidth);
			    Ptr += LineWidth;

				uint8_t *LinePtr = Line;

				for (unsigned int j = 0; j < GetWidth(); j++) {
					if (m_BitmapHeader.BitCount == 1) {
						uint32_t Color = *((uint8_t*) LinePtr);
						for (int k = 0; k < 8; k++) {
							m_BitmapData[Index].Red = ColorTable[Color & 0x80 ? 1 : 0].Red;
							m_BitmapData[Index].Green = ColorTable[Color & 0x80 ? 1 : 0].Green;
							m_BitmapData[Index].Blue = ColorTable[Color & 0x80 ? 1 : 0].Blue;
							m_BitmapData[Index].Alpha = ColorTable[Color & 0x80 ? 1 : 0].Alpha;
							Index++;
							Color <<= 1;
						}
						LinePtr++;
						j += 7;
					} else if (m_BitmapHeader.BitCount == 4) {
						uint32_t Color = *((uint8_t*) LinePtr);
						m_BitmapData[Index].Red = ColorTable[(Color >> 4) & 0x0f].Red;
						m_BitmapData[Index].Green = ColorTable[(Color >> 4) & 0x0f].Green;
						m_BitmapData[Index].Blue = ColorTable[(Color >> 4) & 0x0f].Blue;
						m_BitmapData[Index].Alpha = ColorTable[(Color >> 4) & 0x0f].Alpha;
						Index++;
						m_BitmapData[Index].Red = ColorTable[Color & 0x0f].Red;
						m_BitmapData[Index].Green = ColorTable[Color & 0x0f].Green;
						m_BitmapData[Index].Blue = ColorTable[Color & 0x0f].Blue;
						m_BitmapData[Index].Alpha = ColorTable[Color & 0x0f].Alpha;
						Index++;
						LinePtr++;
						j++;
					} else if (m_BitmapHeader.BitCount == 8) {
						uint32_t Color = *((uint8_t*) LinePtr);
						m_BitmapData[Index].Red = ColorTable[Color].Red;
						m_BitmapData[Index].Green = ColorTable[Color].Green;
						m_BitmapData[Index].Blue = ColorTable[Color].Blue;
						m_BitmapData[Index].Alpha = 255; // As Alpha value of table is alwais zero
						Index++;
						LinePtr++;
					} else if (m_BitmapHeader.BitCount == 16) {
						uint32_t Color = *((uint16_t*) LinePtr);
						m_BitmapData[Index].Red = ((Color >> 10) & 0x1f) << 3;
						m_BitmapData[Index].Green = ((Color >> 5) & 0x1f) << 3;
						m_BitmapData[Index].Blue = (Color & 0x1f) << 3;
						m_BitmapData[Index].Alpha = 255;
						Index++;
						LinePtr += 2;
					} else if (m_BitmapHeader.BitCount == 24) {
                        m_BitmapData[Index].Red   = *LinePtr++;
						m_BitmapData[Index].Green = *LinePtr++;
                        m_BitmapData[Index].Blue  = *LinePtr++;
						m_BitmapData[Index].Alpha = 255;
						Index++;
					} else if (m_BitmapHeader.BitCount == 32) {
						uint32_t Color = *((uint32_t*) LinePtr);
						m_BitmapData[Index].Blue = Color & 0xff;
						m_BitmapData[Index].Green = (Color >> 8) & 0xff;
						m_BitmapData[Index].Red = (Color >> 16) & 0xff;
						m_BitmapData[Index].Alpha = Color >> 24;
						Index++;
						LinePtr += 4;
					}
				}
			}
		} else if (m_BitmapHeader.Compression == 1) { // RLE 8
			uint32_t Count = 0;
			uint32_t ColorIndex = 0;
			uint32_t x = 0, y = 0;

			while((Ptr - Buff) < Sz) {
			    Count = *Ptr++;
			    ColorIndex = *Ptr++;

				if(Count > 0) {
					Index = x + y * GetWidth();
					uint32_t kTop = Index + Count;
					for(uint32_t k = Index; k < kTop; k++) {
						m_BitmapData[k].Red = ColorTable[ColorIndex].Red;
						m_BitmapData[k].Green = ColorTable[ColorIndex].Green;
						m_BitmapData[k].Blue = ColorTable[ColorIndex].Blue;
						m_BitmapData[k].Alpha = 255;
					}
					x += Count;
				}
				else { // Count == 0
				    uint32_t Flag = ColorIndex;
					if(Flag == 0) {
						x = 0;
						y++;
					}
					else if(Flag == 1) break;
					else if(Flag == 2) {
						x += *Ptr++; // rx
						y += *Ptr++; // ry
					}
					else {
						Count = Flag;
						Index = x + y * GetWidth();
						uint32_t kTop = Index + Count;
						for(uint32_t k = Index; k < kTop; k++) {
						    ColorIndex = *Ptr++;
							m_BitmapData[k].Red = ColorTable[ColorIndex].Red;
							m_BitmapData[k].Green = ColorTable[ColorIndex].Green;
							m_BitmapData[k].Blue = ColorTable[ColorIndex].Blue;
							m_BitmapData[k].Alpha = 255;
						}
						x += Count;
						uint32_t pos = Ptr - Buff;
						if(pos & 1) Ptr++;
					}
				}
			}
		} else if (m_BitmapHeader.Compression == 2) { // RLE 4
			/* RLE 4 is not supported */
			Result = false;
		} else if (m_BitmapHeader.Compression == 3) { // BITFIELDS
			/* We assumes that mask of each color component can be in any order */
			uint32_t BitCountRed = CColor::BitCountByMask(m_BitmapHeader.RedMask);
			uint32_t BitCountGreen = CColor::BitCountByMask(m_BitmapHeader.GreenMask);
			uint32_t BitCountBlue = CColor::BitCountByMask(m_BitmapHeader.BlueMask);
			uint32_t BitCountAlpha = CColor::BitCountByMask(m_BitmapHeader.AlphaMask);

			for (unsigned int i = 0; i < GetHeight(); i++) {
			    memcpy(Line, Ptr, LineWidth);
                Ptr += LineWidth;
				uint8_t *LinePtr = Line;
				for (unsigned int j = 0; j < GetWidth(); j++) {
					uint32_t Color = 0;
					if (m_BitmapHeader.BitCount == 16) {
						Color = *((uint16_t*) LinePtr);
						LinePtr += 2;
					} else if (m_BitmapHeader.BitCount == 32) {
						Color = *((uint32_t*) LinePtr);
						LinePtr += 4;
					} else {
						// Other formats are not valid
					}
					m_BitmapData[Index].Red = CColor::Convert(CColor::ComponentByMask(Color, m_BitmapHeader.RedMask), BitCountRed, 8);
					m_BitmapData[Index].Green = CColor::Convert(CColor::ComponentByMask(Color, m_BitmapHeader.GreenMask), BitCountGreen, 8);
					m_BitmapData[Index].Blue = CColor::Convert(CColor::ComponentByMask(Color, m_BitmapHeader.BlueMask), BitCountBlue, 8);
					m_BitmapData[Index].Alpha = CColor::Convert(CColor::ComponentByMask(Color, m_BitmapHeader.AlphaMask), BitCountAlpha, 8);

					Index++;
				}
			}
		}
		return Result;
	}

	unsigned int GetWidth() {
		/* Add plausibility test */
		// if (abs(m_BitmapHeader.Width) > 8192) {
		//	m_BitmapHeader.Width = 8192;
		// }
		return m_BitmapHeader.Width < 0 ? -m_BitmapHeader.Width : m_BitmapHeader.Width;
	}

	unsigned int GetHeight() {
		/* Add plausibility test */
		// if (abs(m_BitmapHeader.Height) > 8192) {
		//	m_BitmapHeader.Height = 8192;
		// }
		return m_BitmapHeader.Height < 0 ? -m_BitmapHeader.Height : m_BitmapHeader.Height;
	}

	unsigned int GetBitCount() {
		/* Add plausibility test */
		// if (m_BitmapHeader.BitCount > 32) {
		//	m_BitmapHeader.BitCount = 32;
		// }
		return m_BitmapHeader.BitCount;
	}

	/* Copies internal RGBA buffer to user specified buffer */

	bool GetBits(void* Buffer, unsigned int &Size) {
		bool Result = false;
		if (Size == 0 || Buffer == 0) {
			Size = m_BitmapSize * sizeof(RGBA_t);
			Result = m_BitmapSize != 0;
		} else {
			memcpy(Buffer, m_BitmapData, Size);
			Result = true;
		}
		return Result;
	}

	/* Returns internal RGBA buffer */

	void* GetBits() {
		return m_BitmapData;
	}

	/* Copies internal RGBA buffer to user specified buffer and converts it into destination
	 * bit format specified by component masks.
	 *
	 * Typical Bitmap color formats (BGR/BGRA):
	 *
	 * Masks for 16 bit (5-5-5): ALPHA = 0x00000000, RED = 0x00007C00, GREEN = 0x000003E0, BLUE = 0x0000001F
	 * Masks for 16 bit (5-6-5): ALPHA = 0x00000000, RED = 0x0000F800, GREEN = 0x000007E0, BLUE = 0x0000001F
	 * Masks for 24 bit: ALPHA = 0x00000000, RED = 0x00FF0000, GREEN = 0x0000FF00, BLUE = 0x000000FF
	 * Masks for 32 bit: ALPHA = 0xFF000000, RED = 0x00FF0000, GREEN = 0x0000FF00, BLUE = 0x000000FF
	 *
	 * Other color formats (RGB/RGBA):
	 *
	 * Masks for 32 bit (RGBA): ALPHA = 0xFF000000, RED = 0x000000FF, GREEN = 0x0000FF00, BLUE = 0x00FF0000
	 *
	 * Bit count will be rounded to next 8 bit boundary. If IncludePadding is true, it will be ensured
	 * that line width is a multiple of 4. padding bytes are included if necessary.
	 *
	 * NOTE: systems with big endian byte order may require masks in inversion order.
	 */

	bool GetBits(void* Buffer, unsigned int &Size, unsigned int RedMask, unsigned int GreenMask, unsigned int BlueMask, unsigned int AlphaMask, bool IncludePadding = true) {
		bool Result = false;

		uint32_t BitCountRed = CColor::BitCountByMask(RedMask);
		uint32_t BitCountGreen = CColor::BitCountByMask(GreenMask);
		uint32_t BitCountBlue = CColor::BitCountByMask(BlueMask);
		uint32_t BitCountAlpha = CColor::BitCountByMask(AlphaMask);

		unsigned int BitCount = (BitCountRed + BitCountGreen + BitCountBlue + BitCountAlpha + 7) & ~7;

		if (BitCount > 32) {
			return false;
		}

		unsigned int w = GetWidth();
		//unsigned int LineWidth = (w + 3) & ~3;
		unsigned int dataBytesPerLine = (w * BitCount + 7) / 8;
		unsigned int LineWidth = (dataBytesPerLine + 3) & ~3;

		if (Size == 0 || Buffer == 0) {
			//Size = (LineWidth * GetHeight() * BitCount) / 8 + sizeof(unsigned int);
			Size = (GetWidth() * GetHeight() * BitCount) / 8 + sizeof(unsigned int);
			return true;
		}

		uint8_t* BufferPtr = (uint8_t*) Buffer;

		Result = true;

		uint32_t BitPosRed = CColor::BitPositionByMask(RedMask);
		uint32_t BitPosGreen = CColor::BitPositionByMask(GreenMask);
		uint32_t BitPosBlue = CColor::BitPositionByMask(BlueMask);
		uint32_t BitPosAlpha = CColor::BitPositionByMask(AlphaMask);

		unsigned int j = 0;

		for (unsigned int i = 0; i < m_BitmapSize; i++) {
			*(uint32_t*) BufferPtr =
			(CColor::Convert(m_BitmapData[i].Blue, 8, BitCountBlue) << BitPosBlue) |
			(CColor::Convert(m_BitmapData[i].Green, 8, BitCountGreen) << BitPosGreen) |
			(CColor::Convert(m_BitmapData[i].Red, 8, BitCountRed) << BitPosRed) |
			(CColor::Convert(m_BitmapData[i].Alpha, 8, BitCountAlpha) << BitPosAlpha);

			if (IncludePadding) {
				j++;
				if (j >= w) {
					for (unsigned int k = 0; k < LineWidth - dataBytesPerLine; k++) {
						BufferPtr += (BitCount >> 3);
					}
					j = 0;
				}
			}

			BufferPtr += (BitCount >> 3);
		}

		Size -= sizeof(unsigned int);

		return Result;
	}

	/* See GetBits().
	 * It creates a corresponding color table (palette) which have to be destroyed by the user after usage.
	 *
	 * Supported Bit depths are: 4, 8
	 *
	 * Todo: Optimize, use optimized palette, do ditehring (see my dithering class), support padding for 4 bit bitmaps
	 */

	bool GetBitsWithPalette(void* Buffer, unsigned int &Size, unsigned int BitCount, BGRA_t* &Palette, unsigned int &PaletteSize, bool OptimalPalette = false, bool IncludePadding = true) {
		bool Result = false;

		if (BitCount > 16) {
			return false;
		}

		unsigned int w = GetWidth();
		unsigned int dataBytesPerLine = (w * BitCount + 7) / 8;
		unsigned int LineWidth = (dataBytesPerLine + 3) & ~3;

		if (Size == 0 || Buffer == 0) {
			Size = (LineWidth * GetHeight() * BitCount) / 8;
			return true;
		}


		if (OptimalPalette) {
			PaletteSize = 0;
			// Not implemented
		} else {
			if (BitCount == 1) {
				PaletteSize = 2;
				// Not implemented: Who need that?
			} else if (BitCount == 4) { // 2:2:1
				PaletteSize = 16;
				Palette = new BGRA_t[PaletteSize];
				for (int r = 0; r < 4; r++) {
					for (int g = 0; g < 2; g++) {
						for (int b = 0; b < 2; b++) {
							Palette[r | g << 2 | b << 3].Red = r ? (r << 6) | 0x3f : 0;
							Palette[r | g << 2 | b << 3].Green = g ? (g << 7) | 0x7f : 0;
							Palette[r | g << 2 | b << 3].Blue = b ? (b << 7) | 0x7f : 0;
							Palette[r | g << 2 | b << 3].Alpha = 0xff;
						}
					}
				}
			} else if (BitCount == 8) { // 3:3:2
				PaletteSize = 256;
				Palette = new BGRA_t[PaletteSize];
				for (int r = 0; r < 8; r++) {
					for (int g = 0; g < 8; g++) {
						for (int b = 0; b < 4; b++) {
							Palette[r | g << 3 | b << 6].Red = r ? (r << 5) | 0x1f : 0;
							Palette[r | g << 3 | b << 6].Green = g ? (g << 5) | 0x1f : 0;
							Palette[r | g << 3 | b << 6].Blue = b ? (b << 6) | 0x3f : 0;
							Palette[r | g << 3 | b << 6].Alpha = 0xff;
						}
					}
				}
			} else if (BitCount == 16) { // 5:5:5
				// Not implemented
			}
		}

		unsigned int j = 0;
		uint8_t* BufferPtr = (uint8_t*) Buffer;

		for (unsigned int i = 0; i < m_BitmapSize; i++) {
			if (BitCount == 1) {
				// Not implemented: Who needs that?
			} else if (BitCount == 4) {
				*BufferPtr = ((m_BitmapData[i].Red >> 6) | (m_BitmapData[i].Green >> 7) << 2 | (m_BitmapData[i].Blue >> 7) << 3) << 4;
				i++;
				*BufferPtr |= (m_BitmapData[i].Red >> 6) | (m_BitmapData[i].Green >> 7) << 2 | (m_BitmapData[i].Blue >> 7) << 3;
			} else if (BitCount == 8) {
				*BufferPtr = (m_BitmapData[i].Red >> 5) | (m_BitmapData[i].Green >> 5) << 3 | (m_BitmapData[i].Blue >> 5) << 6;
			} else if (BitCount == 16) {
				// Not implemented
			}

			if (IncludePadding) {
				j++;
				if (j >= w) {
					for (unsigned int k = 0; k < (LineWidth - dataBytesPerLine); k++) {
						BufferPtr += BitCount / 8;
					}
					j = 0;
				}
			}

			BufferPtr++;
		}

		Result = true;

		return Result;
	}

	/* Set Bitmap Bits. Will be converted to RGBA internally */

	bool SetBits(void* Buffer, unsigned int Width, unsigned int Height, unsigned int RedMask, unsigned int GreenMask, unsigned int BlueMask, unsigned int AlphaMask = 0) {
		if (Buffer == 0) {
			return false;
		}

		uint8_t *BufferPtr = (uint8_t*) Buffer;

		Dispose();

		m_BitmapHeader.Width = Width;
		m_BitmapHeader.Height = Height;
		m_BitmapHeader.BitCount = 32;
		m_BitmapHeader.Compression = 3;

		m_BitmapSize = GetWidth() * GetHeight();
		m_BitmapData = new RGBA_t[m_BitmapSize];

		/* Find bit count by masks (rounded to next 8 bit boundary) */

		unsigned int BitCount = (CColor::BitCountByMask(RedMask | GreenMask | BlueMask | AlphaMask) + 7) & ~7;

		uint32_t BitCountRed = CColor::BitCountByMask(RedMask);
		uint32_t BitCountGreen = CColor::BitCountByMask(GreenMask);
		uint32_t BitCountBlue = CColor::BitCountByMask(BlueMask);
		uint32_t BitCountAlpha = CColor::BitCountByMask(AlphaMask);

		for (unsigned int i = 0; i < m_BitmapSize; i++) {
			unsigned int Color = 0;
			if (BitCount <= 8) {
				Color = *((uint8_t*) BufferPtr);
				BufferPtr += 1;
			} else if (BitCount <= 16) {
				Color = *((uint16_t*) BufferPtr);
				BufferPtr += 2;
			} else if (BitCount <= 24) {
				Color = *((uint32_t*) BufferPtr);
				BufferPtr += 3;
			} else if (BitCount <= 32) {
				Color = *((uint32_t*) BufferPtr);
				BufferPtr += 4;
			} else {
				/* unsupported */
				BufferPtr += 1;
			}
			m_BitmapData[i].Alpha = CColor::Convert(CColor::ComponentByMask(Color, AlphaMask), BitCountAlpha, 8);
			m_BitmapData[i].Red = CColor::Convert(CColor::ComponentByMask(Color, RedMask), BitCountRed, 8);
			m_BitmapData[i].Green = CColor::Convert(CColor::ComponentByMask(Color, GreenMask), BitCountGreen, 8);
			m_BitmapData[i].Blue = CColor::Convert(CColor::ComponentByMask(Color, BlueMask), BitCountBlue, 8);
		}

		return true;
	}
};
