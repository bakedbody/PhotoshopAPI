#pragma once

#include "Macros.h"
#include "Util/Enum.h"
#include "Core/Struct/File.h"
#include "Core/Struct/Section.h"
#include "Core/FileIO/Read.h"
#include "Core/FileIO/Write.h"
#include "Core/Compression/Compress_RLE.h"

#include "blosc2.h"


PSAPI_NAMESPACE_BEGIN


namespace ImageDataImpl
{
	template <typename T>
	void writeCompressedData(File& document, const FileHeader& header, const uint16_t numChannels, std::vector<T>&& uncompressedData)
	{
		if (header.m_Version == Enum::Version::Psd)
		{
			std::vector<uint16_t> scanlineSizes;
			std::vector<uint8_t> compressedData = CompressRLEImageDataPsd(uncompressedData, header.m_Width, header.m_Height, scanlineSizes);
			// First write all the scanline sizes, then the compressed data
			for (int i = 0; i < numChannels; ++i)
			{
				// we must copy here as we otherwise byteswap multiple times
				auto data = scanlineSizes;
				WriteBinaryArray<uint16_t>(document, std::move(data));
			}
			for (int i = 0; i < numChannels; ++i)
			{
				// we must copy here as we otherwise byteswap multiple times
				auto data = compressedData;
				WriteBinaryArray<uint8_t>(document, std::move(data));
			}
		}
		else
		{
			std::vector<uint32_t> scanlineSizes;
			std::vector<uint8_t> compressedData = CompressRLEImageDataPsb(uncompressedData, header.m_Width, header.m_Height, scanlineSizes);
			// First write all the scanline sizes, then the compressed data
			for (int i = 0; i < numChannels; ++i)
			{
				// we must copy here as we otherwise byteswap multiple times
				auto data = scanlineSizes;
				WriteBinaryArray<uint32_t>(document, std::move(data));
			}
			for (int i = 0; i < numChannels; ++i)
			{
				// we must copy here as we otherwise byteswap multiple times
				auto data = compressedData;
				WriteBinaryArray<uint8_t>(document, std::move(data));
			}
		}
	}

	template <typename T>
	void writeRawData(File& document, const FileHeader& header, std::vector<T>&& uncompressedData)
	{
		WriteBinaryArray<T>(document, uncompressedData);
	}
}


/// \brief This section is for interoperability with different software such as lightroom and holds a composite of all the layers
///
/// When writing out data we fill it with empty pixels using Rle compression, this is due to Photoshop unfortunately requiring
/// it to be present. Due to this compression step we can usually save lots of data over what Photoshop writes out
struct ImageData : public FileSection
{

	/// Write out an empty image data section from the number of channels. This section is unfortunately required
	inline void write(File& document, const FileHeader& header)
	{
		if (can_roundtrip(header))
		{
			std::span<uint8_t> rawSpan(m_RawData.data(), m_RawData.size());
			document.write(rawSpan);
			return;
		}

		// Compression marker, we default to RLE compression to reduce the size significantly. The way in which the scanlines are stored
		// is slightly different though. All the channels store their scanline sizes at the start of the ImageData section rather than
		// at the start of each channel
		WriteBinaryData<uint16_t>(document, 1u);
		// Write out empty data for all of the channels
		if (header.m_Depth == Enum::BitDepth::BD_8)
		{
			std::vector<uint8_t> emptyData(static_cast<uint64_t>(header.m_Width) * header.m_Height, 0u);
			ImageDataImpl::writeCompressedData(document, header, m_NumChannels, std::move(emptyData));
		}
		else if (header.m_Depth == Enum::BitDepth::BD_16)
		{
			std::vector<uint16_t> emptyData(static_cast<uint64_t>(header.m_Width) * header.m_Height, 0u);
			ImageDataImpl::writeCompressedData(document, header, m_NumChannels, std::move(emptyData));
		}
		else if (header.m_Depth == Enum::BitDepth::BD_32)
		{
			std::vector<float32_t> emptyData(static_cast<uint64_t>(header.m_Width) * header.m_Height, 0u);
			ImageDataImpl::writeCompressedData(document, header, m_NumChannels, std::move(emptyData));
		}
	}

	inline void read(File& document, const FileHeader& header, const uint64_t offset)
	{
		m_SourceVersion = header.m_Version;
		m_SourceWidth = header.m_Width;
		m_SourceHeight = header.m_Height;
		m_SourceDepth = header.m_Depth;
		m_SourceColorMode = header.m_ColorMode;
		m_SourceNumChannels = header.m_NumChannels;
		m_NumChannels = header.m_NumChannels;

		if (offset >= document.getSize())
		{
			return;
		}

		const uint64_t size = document.getSize() - offset;
		FileSection::initialize(static_cast<size_t>(offset), static_cast<size_t>(size));
		m_RawData.resize(static_cast<size_t>(size));
		document.readFromOffset(std::span<uint8_t>(m_RawData.data(), m_RawData.size()), offset);
		m_HasPreservedData = is_valid_raw_image_data(m_RawData);
	}

	ImageData() = default;

	/// Initialize the ImageData with a given number of channels to write out. We do this rather than deducting
	/// from the header as the header counts alpha channels while this does not!
	ImageData(uint16_t numChannels) : m_NumChannels(numChannels) {};

	bool has_preserved_data() const noexcept { return m_HasPreservedData; }

private:
	inline bool can_roundtrip(const FileHeader& header) const noexcept
	{
		return m_HasPreservedData &&
			header.m_Version == m_SourceVersion &&
			header.m_Width == m_SourceWidth &&
			header.m_Height == m_SourceHeight &&
			header.m_Depth == m_SourceDepth &&
			header.m_ColorMode == m_SourceColorMode &&
			header.m_NumChannels == m_SourceNumChannels;
	}

	static inline bool is_valid_raw_image_data(const std::vector<uint8_t>& rawData) noexcept
	{
		if (rawData.size() < 2u)
		{
			return false;
		}
		const uint16_t compression =
			static_cast<uint16_t>((rawData[0] << 8u) | rawData[1]);
		return compression <= 3u;
	}

	uint16_t m_NumChannels = 0u;
	bool m_HasPreservedData = false;
	std::vector<uint8_t> m_RawData;
	Enum::Version m_SourceVersion = Enum::Version::Psd;
	uint64_t m_SourceWidth = 0u;
	uint64_t m_SourceHeight = 0u;
	Enum::BitDepth m_SourceDepth = Enum::BitDepth::BD_8;
	Enum::ColorMode m_SourceColorMode = Enum::ColorMode::RGB;
	uint16_t m_SourceNumChannels = 0u;
};




PSAPI_NAMESPACE_END
