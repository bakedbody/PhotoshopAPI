#include "doctest.h"

#include "PhotoshopFile/PhotoshopFile.h"
#include "LayeredFile/LayeredFile.h"
#include "Macros.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>


/*
These test cases simply check if we can read -> write -> read again parsing through the LayeredFile struct. Unfortunately these written files do have to be checked by
hand as we internally can read files that Photoshop sometimes cannot.
*/

namespace
{
	std::vector<uint8_t> read_binary_file(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		REQUIRE(stream.good());
		return std::vector<uint8_t>(
			std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>()
		);
	}

	uint16_t read_be16(const std::vector<uint8_t>& bytes, size_t offset)
	{
		REQUIRE(offset + 2u <= bytes.size());
		return static_cast<uint16_t>((bytes[offset] << 8u) | bytes[offset + 1u]);
	}

	uint32_t read_be32(const std::vector<uint8_t>& bytes, size_t offset)
	{
		REQUIRE(offset + 4u <= bytes.size());
		return (static_cast<uint32_t>(bytes[offset]) << 24u) |
			(static_cast<uint32_t>(bytes[offset + 1u]) << 16u) |
			(static_cast<uint32_t>(bytes[offset + 2u]) << 8u) |
			static_cast<uint32_t>(bytes[offset + 3u]);
	}

	int16_t read_i16(const std::vector<uint8_t>& bytes, size_t offset)
	{
		return static_cast<int16_t>(read_be16(bytes, offset));
	}

	int32_t read_i32(const std::vector<uint8_t>& bytes, size_t offset)
	{
		return static_cast<int32_t>(read_be32(bytes, offset));
	}

	uint64_t read_be64(const std::vector<uint8_t>& bytes, size_t offset)
	{
		REQUIRE(offset + 8u <= bytes.size());
		uint64_t value = 0u;
		for (size_t i = 0u; i < 8u; ++i)
		{
			value = (value << 8u) | static_cast<uint64_t>(bytes[offset + i]);
		}
		return value;
	}

	std::vector<uint8_t> read_image_data_tail(const std::filesystem::path& path)
	{
		auto bytes = read_binary_file(path);
		REQUIRE(bytes.size() > 26u);
		REQUIRE(bytes[0] == '8');
		REQUIRE(bytes[1] == 'B');
		REQUIRE(bytes[2] == 'P');
		REQUIRE(bytes[3] == 'S');

		const uint16_t version = read_be16(bytes, 4u);
		size_t offset = 26u;

		const uint32_t color_mode_len = read_be32(bytes, offset);
		offset += 4u + color_mode_len;
		REQUIRE(offset + 4u <= bytes.size());

		const uint32_t image_resource_len = read_be32(bytes, offset);
		offset += 4u + image_resource_len;
		REQUIRE(offset < bytes.size());

		if (version == 1u)
		{
			const uint32_t layer_mask_len = read_be32(bytes, offset);
			offset += 4u + layer_mask_len;
		}
		else
		{
			const uint64_t layer_mask_len = read_be64(bytes, offset);
			offset += 8u + static_cast<size_t>(layer_mask_len);
		}
		REQUIRE(offset < bytes.size());

		return std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
	}

	std::vector<uint8_t> read_first_txt2_block(const std::filesystem::path& path)
	{
		const auto bytes = read_binary_file(path);
		const std::vector<uint8_t> sig_psd = { '8', 'B', 'I', 'M', 'T', 'x', 't', '2' };
		const std::vector<uint8_t> sig_psb = { '8', 'B', '6', '4', 'T', 'x', 't', '2' };

		for (size_t i = 0u; i + 12u <= bytes.size(); ++i)
		{
			const bool is_psd_txt2 = std::equal(sig_psd.begin(), sig_psd.end(), bytes.begin() + static_cast<std::ptrdiff_t>(i));
			const bool is_psb_txt2 = std::equal(sig_psb.begin(), sig_psb.end(), bytes.begin() + static_cast<std::ptrdiff_t>(i));
			if (!is_psd_txt2 && !is_psb_txt2)
			{
				continue;
			}

			const size_t length_offset = i + 8u;
			const uint64_t payload_len = is_psd_txt2
				? static_cast<uint64_t>(read_be32(bytes, length_offset))
				: read_be64(bytes, length_offset);
			const size_t length_size = is_psd_txt2 ? 4u : 8u;
			const size_t payload_offset = length_offset + length_size;
			const size_t padding = payload_len % 2u;
			const size_t block_end = payload_offset + static_cast<size_t>(payload_len) + padding;
			REQUIRE(block_end <= bytes.size());
			return std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(i), bytes.begin() + static_cast<std::ptrdiff_t>(block_end));
		}

		return {};
	}

	size_t pad4(size_t value)
	{
		return (value + 3u) & ~size_t{ 3u };
	}

	struct LayerChannelSummary
	{
		uint16_t count = 0u;
		uint64_t total_size = 0u;
	};

	std::unordered_map<std::string, LayerChannelSummary> read_layer_channel_summaries(const std::filesystem::path& path)
	{
		const auto bytes = read_binary_file(path);
		REQUIRE(bytes.size() > 26u);
		REQUIRE(bytes[0] == '8');
		REQUIRE(bytes[1] == 'B');
		REQUIRE(bytes[2] == 'P');
		REQUIRE(bytes[3] == 'S');
		REQUIRE(read_be16(bytes, 4u) == 1u);

		size_t offset = 26u;
		offset += 4u + read_be32(bytes, offset);
		offset += 4u + read_be32(bytes, offset);
		REQUIRE(offset + 8u <= bytes.size());

		const uint32_t layer_mask_len = read_be32(bytes, offset);
		offset += 4u;
		REQUIRE(layer_mask_len > 0u);
		REQUIRE(offset + layer_mask_len <= bytes.size());

		const uint32_t layer_info_len = read_be32(bytes, offset);
		offset += 4u;
		REQUIRE(layer_info_len > 0u);
		const size_t layer_info_end = offset + layer_info_len;
		REQUIRE(layer_info_end <= bytes.size());

		const uint16_t layer_count = static_cast<uint16_t>(std::abs(read_i16(bytes, offset)));
		offset += 2u;

		std::unordered_map<std::string, LayerChannelSummary> summaries;
		for (uint16_t i = 0u; i < layer_count; ++i)
		{
			offset += 16u;
			REQUIRE(offset + 2u <= layer_info_end);
			const uint16_t channel_count = read_be16(bytes, offset);
			offset += 2u;

			uint64_t channel_total = 0u;
			for (uint16_t channel = 0u; channel < channel_count; ++channel)
			{
				offset += 2u;
				channel_total += read_be32(bytes, offset);
				offset += 4u;
			}

			offset += 12u;
			const uint32_t extra_len = read_be32(bytes, offset);
			offset += 4u;
			const size_t extra_start = offset;

			const uint32_t mask_len = read_be32(bytes, offset);
			offset += 4u + mask_len;
			const uint32_t blending_len = read_be32(bytes, offset);
			offset += 4u + blending_len;

			const uint8_t name_len = bytes[offset];
			REQUIRE(offset + 1u + name_len <= bytes.size());
			std::string name(
				reinterpret_cast<const char*>(bytes.data() + offset + 1u),
				reinterpret_cast<const char*>(bytes.data() + offset + 1u + name_len)
			);
			summaries[name] = LayerChannelSummary{ channel_count, channel_total };

			offset = extra_start + extra_len;
			REQUIRE(offset <= layer_info_end);
		}

		return summaries;
	}
}


void checkFileRoundtripping(const std::filesystem::path& inDir, const std::filesystem::path& outDir, const std::filesystem::path& psFile)
{
	using namespace NAMESPACE_PSAPI;

	std::filesystem::path fullInPath = inDir / psFile;
	std::filesystem::path fullOutPath = outDir / psFile;
	ProgressCallback callback;

	// Load the input file
	auto inputFile = File(fullInPath);
	auto psDocumentPtr = std::make_unique<PhotoshopFile>();
	psDocumentPtr->read(inputFile, callback);
	if (psDocumentPtr->m_Header.m_Depth == Enum::BitDepth::BD_8)
	{
		LayeredFile<bpp8_t> layeredFile = { std::move(psDocumentPtr), fullInPath };

		// Write to disk
		File::FileParams params = File::FileParams();
		params.doRead = false;
		params.forceOverwrite = true;
		auto outputFile = File(fullOutPath, params);
		auto psdOutDocumentPtr = layered_to_photoshop(std::move(layeredFile), fullOutPath);
		psdOutDocumentPtr->write(outputFile, callback);

		// Read back into LayeredFile
		auto inputFileRoundtripped = File(fullOutPath);
		auto psDocumentPtrRoundtripped = std::make_unique<PhotoshopFile>();
		psDocumentPtrRoundtripped->read(inputFileRoundtripped, callback);

		LayeredFile<bpp8_t> layeredFileRoundtripped = { std::move(psDocumentPtrRoundtripped), fullOutPath };
	}
	else if (psDocumentPtr->m_Header.m_Depth == Enum::BitDepth::BD_16)
	{
		LayeredFile<bpp16_t> layeredFile = { std::move(psDocumentPtr), fullInPath };

		// Write to disk
		File::FileParams params = File::FileParams();
		params.doRead = false;
		params.forceOverwrite = true;
		auto outputFile = File(fullOutPath, params);
		auto psdOutDocumentPtr = layered_to_photoshop(std::move(layeredFile), fullOutPath);
		psdOutDocumentPtr->write(outputFile, callback);

		// Read back into LayeredFile
		auto inputFileRoundtripped = File(fullOutPath);
		auto psDocumentPtrRoundtripped = std::make_unique<PhotoshopFile>();
		psDocumentPtrRoundtripped->read(inputFileRoundtripped, callback);

		LayeredFile<bpp16_t> layeredFileRoundtripped = { std::move(psDocumentPtrRoundtripped), fullOutPath };
	}
	else if (psDocumentPtr->m_Header.m_Depth == Enum::BitDepth::BD_32)
	{
		LayeredFile<bpp32_t> layeredFile = { std::move(psDocumentPtr), fullInPath };

		// Write to disk
		File::FileParams params = File::FileParams();
		params.doRead = false;
		params.forceOverwrite = true;
		auto outputFile = File(fullOutPath, params);
		auto psdOutDocumentPtr = layered_to_photoshop(std::move(layeredFile), fullOutPath);
		psdOutDocumentPtr->write(outputFile, callback);

		// Read back into LayeredFile
		auto inputFileRoundtripped = File(fullOutPath);
		auto psDocumentPtrRoundtripped = std::make_unique<PhotoshopFile>();
		psDocumentPtrRoundtripped->read(inputFileRoundtripped, callback);

		LayeredFile<bpp32_t> layeredFileRoundtripped = { std::move(psDocumentPtrRoundtripped), fullOutPath };
	}
}


TEST_CASE("Check Roundtripping Compression")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"Compression_Mixed_8bit.psd",
		"Compression_Mixed_8bit.psd",
		"Compression_RAW_8bit.psb",
		"Compression_RAW_8bit.psd",
		"Compression_RLE_8bit.psb",
		"Compression_RLE_8bit.psd",
		"Compression_ZipPrediction_MaximizeCompatibilityOff_16bit.psb",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"Compression_ZipPrediction_MaximizeCompatibilityOff_16bit.psd",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"Compression_ZipPrediction_MaximizeCompatibilityOff_32bit.psb",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"Compression_ZipPrediction_MaximizeCompatibilityOff_32bit.psd",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"Compression_ZipPrediction_16bit.psb",
		"Compression_ZipPrediction_16bit.psd",
		"Compression_ZipPrediction_32bit.psb",
		"Compression_ZipPrediction_32bit.psd"
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/Compression";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Check Roundtripping DPI")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"300dpi.psd",
		"300_point_5_dpi.psd",
		"700dpi.psd"
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/DPI";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Check Roundtripping Groups")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"Groups_8bit.psb",
		"Groups_8bit.psd",
		"Groups_16bit.psb",
		"Groups_16bit.psd",
		"Groups_32bit.psb",
		"Groups_32bit.psd"
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/Groups";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Check Roundtripping ICCProfiles")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"AdobeRGB1998.psb",
		"AppleRGB.psb",
		"CIERGB.psb"
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/ICCProfiles";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Check Roundtripping Masks")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"SingleLayer_8bit.psb",
		"SingleLayer_8bit.psd",
		"SingleLayer_8bit_MaximizeCompatibilityOff.psb",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"SingleLayer_8bit_MaximizeCompatibilityOff.psd",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"SingleLayer_16bit.psb",
		"SingleLayer_16bit.psd",
		"SingleLayer_16bit_MaximizeCompatibilityOff.psb",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"SingleLayer_16bit_MaximizeCompatibilityOff.psd",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"SingleLayer_32bit.psb",
		"SingleLayer_32bit.psd",
		"SingleLayer_32bit_MaximizeCompatibilityOff.psb",	// The MaximizeCompatibility setting will be ignored by us but it is another test case
		"SingleLayer_32bit_MaximizeCompatibilityOff.psd"	// The MaximizeCompatibility setting will be ignored by us but it is another test case
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/SingleLayer";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Check Roundtripping CMYK")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"CMYK_8.psd",
		"CMYK_8.psb",
		"CMYK_16.psd",
		"CMYK_16.psb"
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/CMYK";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Check Roundtripping Grayscale")
{
	static std::vector<std::filesystem::path> fileNames =
	{
		"Grayscale_8.psd",
		"Grayscale_8.psb",
		"Grayscale_16.psd",
		"Grayscale_16.psb",
		"Grayscale_32.psd",
		"Grayscale_32.psb"
	};

	const std::filesystem::path inDir = std::filesystem::current_path() / "documents/Grayscale";
	const std::filesystem::path outDir = std::filesystem::current_path() / "documents/TestRoundtrippingOutput";

	for (const auto& fileName : fileNames)
	{
		checkFileRoundtripping(inDir, outDir, fileName);
	}
}


TEST_CASE("Roundtripping preserves final ImageData composite bytes")
{
	using namespace NAMESPACE_PSAPI;

	const auto fixture = std::filesystem::current_path() / "documents" / "TextLayers" / "TextLayers_Basic.psd";
	const auto tmp = std::filesystem::temp_directory_path() / "psapi_image_data_roundtrip.psd";

	const auto original_image_data = read_image_data_tail(fixture);
	REQUIRE(original_image_data.size() > 2u);

	auto file = LayeredFile<bpp8_t>::read(fixture);
	LayeredFile<bpp8_t>::write(std::move(file), tmp);

	const auto roundtripped_image_data = read_image_data_tail(tmp);
	CHECK(roundtripped_image_data == original_image_data);

	std::filesystem::remove(tmp);
}


TEST_CASE("Roundtripping preserves global Txt2 block")
{
	using namespace NAMESPACE_PSAPI;

	const auto fixture = std::filesystem::current_path() / "documents" / "TextLayers" / "TextLayers_Basic.psd";
	const auto tmp = std::filesystem::temp_directory_path() / "psapi_txt2_roundtrip.psd";

	const auto original_txt2 = read_first_txt2_block(fixture);
	REQUIRE(original_txt2.size() > 12u);

	auto file = LayeredFile<bpp8_t>::read(fixture);
	LayeredFile<bpp8_t>::write(std::move(file), tmp);

	const auto roundtripped_txt2 = read_first_txt2_block(tmp);
	CHECK(roundtripped_txt2 == original_txt2);

	std::filesystem::remove(tmp);
}


TEST_CASE("Roundtripping preserves text layer channel image data")
{
	using namespace NAMESPACE_PSAPI;

	const auto fixture = std::filesystem::current_path() / "documents" / "TextLayers" / "TextLayers_Basic.psd";
	const auto tmp = std::filesystem::temp_directory_path() / "psapi_text_layer_channels_roundtrip.psd";

	const auto original_channels = read_layer_channel_summaries(fixture);
	REQUIRE(original_channels.contains("SimpleASCII"));
	REQUIRE(original_channels.contains("MultilineUTF8"));
	REQUIRE(original_channels.at("SimpleASCII").count > 0u);
	REQUIRE(original_channels.at("MultilineUTF8").count > 0u);

	auto file = LayeredFile<bpp8_t>::read(fixture);
	LayeredFile<bpp8_t>::write(std::move(file), tmp);

	const auto roundtripped_channels = read_layer_channel_summaries(tmp);
	REQUIRE(roundtripped_channels.contains("SimpleASCII"));
	REQUIRE(roundtripped_channels.contains("MultilineUTF8"));
	CHECK(roundtripped_channels.at("SimpleASCII").count == original_channels.at("SimpleASCII").count);
	CHECK(roundtripped_channels.at("MultilineUTF8").count == original_channels.at("MultilineUTF8").count);
	CHECK(roundtripped_channels.at("SimpleASCII").total_size > 0u);
	CHECK(roundtripped_channels.at("MultilineUTF8").total_size > 0u);

	std::filesystem::remove(tmp);
}
