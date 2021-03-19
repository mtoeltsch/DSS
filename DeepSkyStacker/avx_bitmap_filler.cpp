#include "StdAfx.h"
#include "BitMapFiller.h"
#include "avx_bitmap_filler.h"
#include "avx_support.h"
#include <immintrin.h>


AvxBitmapFiller::AvxBitmapFiller(CMemoryBitmap* pB, CDSSProgress* pP) :
	BitmapFillerInterface{ pB, pP },
	redScale{ 1.0f },
	greenScale{ 1.0f },
	blueScale{ 1.0f },
	cfaType{ CFATYPE_NONE },
	isGray{ true },
	width{ 0 },
	height{ 0 },
	nrLinesWritten{ 0 },
	bytesPerChannel{ 0 },
	redBuffer{},
	greenBuffer{},
	blueBuffer{},
	cfaFactors{ 1.0f, 1.0f, 1.0f, 1.0f },
	sourceBuffer{}
{}

void AvxBitmapFiller::SetWhiteBalance(double fRedScale, double fGreenScale, double fBlueScale)
{
	this->redScale = static_cast<float>(fRedScale);
	this->greenScale = static_cast<float>(fGreenScale);
	this->blueScale = static_cast<float>(fBlueScale);
	setCfaFactors();
}

void AvxBitmapFiller::SetCFAType(CFATYPE cfaTp)
{
	this->cfaType = cfaTp;
	if (auto* pGray16Bitmap = dynamic_cast<C16BitGrayBitmap*>(pBitmap))
		pGray16Bitmap->SetCFAType(cfaTp);
	setCfaFactors();
}

void AvxBitmapFiller::setCfaFactors()
{
	const auto setFactors = [this](const float f0, const float f1, const float f2, const float f3) -> void
	{
		this->cfaFactors.assign({ f0, f1, f2, f3 });
	};

	switch (cfaType)
	{
	case CFATYPE_BGGR: return setFactors(blueScale, greenScale, greenScale, redScale);
	case CFATYPE_GRBG: return setFactors(greenScale, redScale, blueScale, greenScale);
	case CFATYPE_GBRG: return setFactors(greenScale, blueScale, redScale, greenScale);
	case CFATYPE_RGGB: return setFactors(redScale, greenScale, greenScale, blueScale);
	}
};

bool AvxBitmapFiller::isRgbBayerPattern() const
{
	switch (this->cfaType)
	{
	case CFATYPE_BGGR:
	case CFATYPE_GRBG:
	case CFATYPE_GBRG:
	case CFATYPE_RGGB: return true;
	}
	return false;
}

void AvxBitmapFiller::setGrey(bool grey)
{
	this->isGray = grey;
}

void AvxBitmapFiller::setWidth(LONG width)
{
	this->width = width;
}

void AvxBitmapFiller::setHeight(LONG height)
{
	this->height = height;
	if (pProgress != nullptr)
		pProgress->Start2(nullptr, pBitmap->Height());
}

void AvxBitmapFiller::setMaxColors(LONG maxcolors)
{
	this->bytesPerChannel = maxcolors > 255 ? 2 : 1;
}

size_t AvxBitmapFiller::Write(const void* source, size_t bytesPerPixel, size_t nrPixels)
{
	const auto adjustColor = [](const float color, const float adjustFactor) -> float
	{
		constexpr float Maximum = static_cast<float>(std::numeric_limits<std::uint16_t>::max() - 1);
		return std::min(color * adjustFactor, Maximum);
	};

	ZASSERTSTATE(0 != this->width);
	ZASSERTSTATE(0 != this->height);
	ZASSERTSTATE(0 != this->bytesPerChannel);
	ZASSERTSTATE((nrPixels % static_cast<size_t>(this->width)) == 0);

	if (this->isGray)
	{
		ZASSERTSTATE(bytesPerPixel == this->bytesPerChannel);
		constexpr size_t vectorLen = 16;
		redBuffer.resize(nrPixels);
		float* pBuf = redBuffer.data();

		const size_t nrVectors = nrPixels / vectorLen;
		if (this->bytesPerChannel == 1)
		{
			const std::uint8_t* pData = static_cast<const std::uint8_t*>(source);
			for (size_t n = 0; n < nrVectors; ++n, pData += 16, pBuf += 16)
			{
				const __m128i epu8 = _mm_loadu_si128((const __m128i*)pData); // Load 16 pixels (each 8 bits)
				const __m256i epu16 = _mm256_slli_epi16(_mm256_cvtepu8_epi16(epu8), 8);
				const __m256 lo8 = AvxSupport::wordToPackedFloat(_mm256_castsi256_si128(epu16));
				const __m256 hi8 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(epu16, 1));
				_mm256_storeu_ps(pBuf, lo8);
				_mm256_storeu_ps(pBuf + 8, hi8);
			}
			for (size_t i = nrVectors * vectorLen; i < nrPixels; ++i, ++pData, ++pBuf) // Remaining pixels of line
				*pBuf = static_cast<float>(static_cast<std::uint16_t>(*pData) << 8);
		}
		else
		{
			const std::uint16_t* pData = static_cast<const std::uint16_t*>(source);
			for (size_t n = 0; n < nrVectors; ++n, pData += 16, pBuf += 16)
			{
				const __m256i epu16_be = _mm256_loadu_si256((const __m256i*)pData); // Load 16 pixels (each 16 bits big endian)
				const __m256i epu16_le = _mm256_shuffle_epi8(epu16_be,
					_mm256_set_epi32(0x0e0f0c0d, 0x0a0b0809, 0x06070405, 0x02030001, 0x0e0f0c0d, 0x0a0b0809, 0x06070405, 0x02030001)); // big endian -> little endian
				const __m256 lo8 = AvxSupport::wordToPackedFloat(_mm256_castsi256_si128(epu16_le));
				const __m256 hi8 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(epu16_le, 1));
				_mm256_storeu_ps(pBuf, lo8);
				_mm256_storeu_ps(pBuf + 8, hi8);
			}
			for (size_t i = nrVectors * vectorLen; i < nrPixels; ++i, ++pData, ++pBuf) // Remaining pixels of line
				*pBuf = static_cast<float>(_load_be_u16(pData)); // Load an convert to little endian
		}

		if (this->isRgbBayerPattern())
		{
			const size_t y = 2 * (this->nrLinesWritten % 2); // 0, 2, 0, 2, ...
			const float adjustFactors[2] = { this->cfaFactors[y], this->cfaFactors[y + 1] }; // {0, 1} or {2, 3}, depending on the line number.
			const __m256 adjustFactorsVec = _mm256_setr_ps(cfaFactors[y], cfaFactors[y + 1], cfaFactors[y], cfaFactors[y + 1], cfaFactors[y], cfaFactors[y + 1], cfaFactors[y], cfaFactors[y + 1]);
			pBuf = redBuffer.data();
			for (size_t i = 0; i < nrPixels / 8; ++i, pBuf += 8)
			{
				const __m256 value = _mm256_loadu_ps(pBuf);
				const __m256 adjusted = _mm256_mul_ps(value, adjustFactorsVec);
				const __m256 limited = _mm256_min_ps(adjusted, _mm256_set1_ps(static_cast<float>(std::numeric_limits<std::uint16_t>::max() - 1)));
				_mm256_storeu_ps(pBuf, limited);
			}
			for (size_t i = (nrPixels / 8) * 8; i < nrPixels; ++i, ++pBuf) // Remaining pixels of line
				*pBuf = adjustColor(*pBuf, adjustFactors[i % 2]);
		}

		auto* pGray16Bitmap = dynamic_cast<C16BitGrayBitmap*>(pBitmap);
		ZASSERTSTATE(pGray16Bitmap != nullptr);
		pBuf = redBuffer.data();
		std::uint16_t* pOut = pGray16Bitmap->m_vPixels.data() + this->nrLinesWritten * nrPixels;
		for (size_t i = 0; i < nrPixels / 8; ++i, pBuf += 8, pOut += 8)
			_mm_storeu_si128((__m128i*)pOut, AvxSupport::cvtTruncatePsEpu16(_mm256_loadu_ps(pBuf)));
		for (size_t i = (nrPixels / 8) * 8; i < nrPixels; ++i, ++pBuf, ++pOut) // Remaining pixels of line
			*pOut = static_cast<std::uint16_t>(*pBuf);
	}
	else
	{
		ZASSERTSTATE(bytesPerPixel == this->bytesPerChannel * 3);
		redBuffer.resize(nrPixels);
		greenBuffer.resize(nrPixels);
		blueBuffer.resize(nrPixels);
		float* pRed = redBuffer.data();
		float* pGreen = greenBuffer.data();
		float* pBlue = blueBuffer.data();

		if (this->bytesPerChannel == 1)
		{
			sourceBuffer.resize(nrPixels * 3 + 32); // To avoid read access violations.
			memcpy(sourceBuffer.data(), source, nrPixels * 3);
			const std::uint8_t* pData = static_cast<const std::uint8_t*>(sourceBuffer.data());
			const __m256i indices = _mm256_set_epi32(0xffff09ff, 0xffff06ff, 0xffff03ff, 0xffff00ff, 0xffff09ff, 0xffff06ff, 0xffff03ff, 0xffff00ff);
			// 8 RGB pixels at once (each 8 bits)
			for (size_t n = 0; n < nrPixels / 8; ++n, pData += 24, pRed += 8, pGreen += 8, pBlue += 8)
			{
				const __m256i data = _mm256_loadu_si256((const __m256i*)pData); // Load 8 RGB pixels of 8 bits per channel (24 bytes), 8 bytes are ignored.
				const __m256i rgb = _mm256_permute2x128_si256(data, AvxSupport::shiftLeftEpi32<1>(data), 0x30); // 4 pixels (rgb) in lo lane, 4 pixels in hi lane (4 bytes gap ignored).
				// We make 3 steps: for red, green, and blue.
				// In each step, we directly convert the 4 color bytes to 4 ints in each 128 bit lane, then to 4 floats.
				// First step: Red channel @ byte positions 9, 6, 3, 0 in each lane.
				const __m256i red32 = _mm256_shuffle_epi8(rgb, indices); // 4 red values per lane as int32 (original value * 256)
				const __m256i green32 = _mm256_shuffle_epi8(_mm256_srli_si256(rgb, 1), indices); // 8 x green
				const __m256i blue32 = _mm256_shuffle_epi8(_mm256_srli_si256(rgb, 2), indices); // 8 x blue
				// Save the color values:
				_mm256_storeu_ps(pRed, _mm256_cvtepi32_ps(red32));
				_mm256_storeu_ps(pGreen, _mm256_cvtepi32_ps(green32));
				_mm256_storeu_ps(pBlue, _mm256_cvtepi32_ps(blue32));
			}
			for (size_t i = (nrPixels / 8) * 8; i < nrPixels; ++i, ++pRed, ++pGreen, ++pBlue, pData += 3)
			{
				*pRed = static_cast<float>(static_cast<std::uint16_t>(pData[0]) << 8);
				*pGreen = static_cast<float>(static_cast<std::uint16_t>(pData[1]) << 8);
				*pBlue = static_cast<float>(static_cast<std::uint16_t>(pData[2]) << 8);
			}
		}
		else
		{
			sourceBuffer.resize(nrPixels * 6 + 32); // To avoid read access violations.
			memcpy(sourceBuffer.data(), source, nrPixels * 6);
			const std::uint16_t* pData = reinterpret_cast<const std::uint16_t*>(sourceBuffer.data());
			const __m256i indices = _mm256_set_epi32(0xffffffff, 0xffffffff, 0xffff0607, 0xffff0001, 0xffffffff, 0xffffffff, 0xffff0607, 0xffff0001);
			// 4 RGB pixels at once (each 16 bits)
			for (size_t n = 0; n < nrPixels / 4; ++n, pData += 12, pRed += 4, pGreen += 4, pBlue += 4)
			{
				const __m256i data = _mm256_loadu_si256((const __m256i*)pData); // Load 4 RGB pixels of 16 bits per channel (24 bytes), 8 bytes are ignored.
				const __m256i rgb = _mm256_permute2x128_si256(data, AvxSupport::shiftLeftEpi32<1>(data), 0x30); // 2 pixels (rgb) in lo lane, 2 pixels in hi lane (4 bytes gap ignored).
				// 3 steps (R, G, B): Convert the 2 color WORDs to 2 ints in each 128 bit lane, be->le, then convert to float.
				const __m256i red32 = _mm256_shuffle_epi8(rgb, indices); // 2 red values per lane as int32 (original value converted be -> le)
				const __m256i green32 = _mm256_shuffle_epi8(_mm256_srli_si256(rgb, 2), indices); // 4 x green
				const __m256i blue32 = _mm256_shuffle_epi8(_mm256_srli_si256(rgb, 4), indices); // 4 x blue
				// Pack the 2 pixels in the hi lane and the 2 pixels in the lo lane, then convert to float, and store.
				_mm_storeu_ps(pRed, _mm_cvtepi32_ps(_mm256_castsi256_si128(_mm256_permute4x64_epi64(red32, 0x08))));
				_mm_storeu_ps(pGreen, _mm_cvtepi32_ps(_mm256_castsi256_si128(_mm256_permute4x64_epi64(green32, 0x08))));
				_mm_storeu_ps(pBlue, _mm_cvtepi32_ps(_mm256_castsi256_si128(_mm256_permute4x64_epi64(blue32, 0x08))));
			}
			for (size_t i = (nrPixels / 4) * 4; i < nrPixels; ++i, ++pRed, ++pGreen, ++pBlue, pData += 3)
			{
				*pRed = static_cast<float>(_load_be_u16(pData));
				*pGreen = static_cast<float>(_load_be_u16(pData + 1));
				*pBlue = static_cast<float>(_load_be_u16(pData + 2));
			}
		}

		pRed = redBuffer.data();
		pGreen = greenBuffer.data();
		pBlue = blueBuffer.data();
		const __m256 MAXIMUM = _mm256_set1_ps(static_cast<float>(std::numeric_limits<std::uint16_t>::max() - 1));
		for (size_t n = 0; n < nrPixels / 8; ++n, pRed += 8, pGreen += 8, pBlue += 8)
		{
			const __m256 r = _mm256_mul_ps(_mm256_loadu_ps(pRed), _mm256_set1_ps(redScale));
			const __m256 g = _mm256_mul_ps(_mm256_loadu_ps(pGreen), _mm256_set1_ps(greenScale));
			const __m256 b = _mm256_mul_ps(_mm256_loadu_ps(pBlue), _mm256_set1_ps(blueScale));
			_mm256_storeu_ps(pRed, _mm256_min_ps(r, MAXIMUM));
			_mm256_storeu_ps(pGreen, _mm256_min_ps(g, MAXIMUM));
			_mm256_storeu_ps(pBlue, _mm256_min_ps(b, MAXIMUM));
		}
		for (size_t i = (nrPixels / 8) * 8; i < nrPixels; ++i, ++pRed, ++pGreen, ++pBlue)
		{
			*pRed = adjustColor(*pRed, redScale);
			*pGreen = adjustColor(*pGreen, greenScale);
			*pBlue = adjustColor(*pBlue, blueScale);
		}

		auto* pColor16Bitmap = dynamic_cast<C48BitColorBitmap*>(pBitmap);
		ZASSERTSTATE(pColor16Bitmap != nullptr);
		std::uint16_t* pOutRed = pColor16Bitmap->m_Red.m_vPixels.data() + this->nrLinesWritten * nrPixels;
		std::uint16_t* pOutGreen = pColor16Bitmap->m_Green.m_vPixels.data() + this->nrLinesWritten * nrPixels;
		std::uint16_t* pOutBlue = pColor16Bitmap->m_Blue.m_vPixels.data() + this->nrLinesWritten * nrPixels;
		pRed = redBuffer.data();
		pGreen = greenBuffer.data();
		pBlue = blueBuffer.data();
		for (size_t n = 0; n < nrPixels / 8; ++n, pOutRed += 8, pOutGreen += 8, pOutBlue += 8, pRed += 8, pGreen += 8, pBlue += 8)
		{
			_mm_storeu_si128((__m128i*)pOutRed, AvxSupport::cvtTruncatePsEpu16(_mm256_loadu_ps(pRed)));
			_mm_storeu_si128((__m128i*)pOutGreen, AvxSupport::cvtTruncatePsEpu16(_mm256_loadu_ps(pGreen)));
			_mm_storeu_si128((__m128i*)pOutBlue, AvxSupport::cvtTruncatePsEpu16(_mm256_loadu_ps(pBlue)));
		}
		for (size_t i = (nrPixels / 8) * 8; i < nrPixels; ++i, ++pOutRed, ++pOutGreen, ++pOutBlue, ++pRed, ++pGreen, ++pBlue)
		{
			*pOutRed = static_cast<std::uint16_t>(*pRed);
			*pOutGreen = static_cast<std::uint16_t>(*pGreen);
			*pOutBlue = static_cast<std::uint16_t>(*pBlue);
		}
	}

	++this->nrLinesWritten;
	return nrPixels;
}