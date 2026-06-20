#include "doctest.h"

#include "PhotoshopFile/PhotoshopFile.h"
#include "LayeredFile/LayeredFile.h"
#include "LayeredFile/LayerTypes/ImageLayer.h"
#include "Macros.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>


namespace
{
	std::vector<uint8_t> read_binary_file(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		return std::vector<uint8_t>(
			std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>());
	}

	uint32_t read_u32_be(const std::vector<uint8_t>& data, size_t offset)
	{
		return
			(static_cast<uint32_t>(data[offset]) << 24u) |
			(static_cast<uint32_t>(data[offset + 1u]) << 16u) |
			(static_cast<uint32_t>(data[offset + 2u]) << 8u) |
			static_cast<uint32_t>(data[offset + 3u]);
	}

	size_t find_luni_block(const std::vector<uint8_t>& data)
	{
		for (size_t i = 0u; i + 8u < data.size(); ++i)
		{
			if (data[i] == 'l' && data[i + 1u] == 'u' && data[i + 2u] == 'n' && data[i + 3u] == 'i')
			{
				return i;
			}
		}
		return std::string::npos;
	}
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Written unicode layer name omits trailing null terminator")
{
	using namespace NAMESPACE_PSAPI;
	using type = bpp8_t;
	constexpr int32_t width = 2;
	constexpr int32_t height = 2;
	constexpr int32_t size = width * height;

	LayeredFile<type> file(Enum::ColorMode::RGB, width, height);
	std::unordered_map<int, std::vector<type>> data =
	{
		{0, std::vector<type>(size, 0)},
		{1, std::vector<type>(size, 0)},
		{2, std::vector<type>(size, 0)},
	};
	auto params = typename Layer<type>::Params
	{
		.name = "bg",
		.width = width,
		.height = height,
		.colormode = Enum::ColorMode::RGB,
	};
	file.add_layer(std::make_shared<ImageLayer<type>>(data, params));

	const auto path = std::filesystem::temp_directory_path() / "psapi_luni_no_null.psd";
	LayeredFile<type>::write(std::move(file), path);
	const auto bytes = read_binary_file(path);
	std::filesystem::remove(path);

	const auto luni_offset = find_luni_block(bytes);
	REQUIRE(luni_offset != std::string::npos);
	REQUIRE(luni_offset + 16u <= bytes.size());
	CHECK(read_u32_be(bytes, luni_offset + 4u) == 8u);
	CHECK(read_u32_be(bytes, luni_offset + 8u) == 2u);
	CHECK(bytes[luni_offset + 12u] == 0u);
	CHECK(bytes[luni_offset + 13u] == 'b');
	CHECK(bytes[luni_offset + 14u] == 0u);
	CHECK(bytes[luni_offset + 15u] == 'g');
}



// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Read Unicode layer name from psd file")
{
	using namespace NAMESPACE_PSAPI;

	std::filesystem::path psd_path = std::filesystem::current_path();
	psd_path += "/documents/UnicodeNames/UnicodeLayerNames.psd";

	LayeredFile<bpp8_t> layeredFile = LayeredFile<bpp8_t>::read(psd_path);

	// Find the three layers by their names and check if the result is not null
	SUBCASE("Find chinese simplified layer")
	{
		auto ptr = layeredFile.find_layer("Chinese_Simplified/请问可以修改psd 的画板尺寸吗");
		CHECK(ptr);
	}
	SUBCASE("Find overflow layer")
	{
		auto ptr = layeredFile.find_layer("äüöUnicodeNameOverflowPascalString--------------------------------------------------------------------------------------------------------------------");
		CHECK(ptr);
	}
	SUBCASE("Find unicode layer")
	{
		auto ptr = layeredFile.find_layer("UnicodeNameäää");
		CHECK(ptr);
	}
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Read Unicode layer name from psb file")
{
	using namespace NAMESPACE_PSAPI;

	std::filesystem::path psb_path = std::filesystem::current_path();
	psb_path += "/documents/UnicodeNames/UnicodeLayerNames.psb";

	LayeredFile<bpp8_t> layeredFile = LayeredFile<bpp8_t>::read(psb_path);

	// Find the three layers by their names and check if the result is not null
	SUBCASE("Find chinese simplified layer")
	{
		auto ptr = layeredFile.find_layer("Chinese_Simplified/请问可以修改psd 的画板尺寸吗");
		CHECK(ptr);
	}
	SUBCASE("Find overflow layer")
	{
		auto ptr = layeredFile.find_layer("äüöUnicodeNameOverflowPascalString--------------------------------------------------------------------------------------------------------------------");
		CHECK(ptr);
	}
	SUBCASE("Find unicode layer")
	{
		auto ptr = layeredFile.find_layer("UnicodeNameäää");
		CHECK(ptr);
	}
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Read write unicode layer name from psd file")
{
	using namespace NAMESPACE_PSAPI;

	std::filesystem::path psd_path = std::filesystem::current_path();
	psd_path += "/documents/UnicodeNames/UnicodeLayerNames.psd";

	{
		LayeredFile<bpp8_t> layeredFile = LayeredFile<bpp8_t>::read(psd_path);
		LayeredFile<bpp8_t>::write(std::move(layeredFile), psd_path);
	}
	LayeredFile<bpp8_t> layeredFile = LayeredFile<bpp8_t>::read(psd_path);


	// Find the three layers by their names and check if the result is not null
	SUBCASE("Find chinese simplified layer")
	{
		auto ptr = layeredFile.find_layer("Chinese_Simplified/请问可以修改psd 的画板尺寸吗");
		CHECK(ptr);
	}
	SUBCASE("Find overflow layer")
	{
		auto ptr = layeredFile.find_layer("äüöUnicodeNameOverflowPascalString--------------------------------------------------------------------------------------------------------------------");
		CHECK(ptr);
	}
	SUBCASE("Find unicode layer")
	{
		auto ptr = layeredFile.find_layer("UnicodeNameäää");
		CHECK(ptr);
	}
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Read write unicode layer name from psb file")
{
	using namespace NAMESPACE_PSAPI;

	std::filesystem::path psb_path = std::filesystem::current_path();
	psb_path += "/documents/UnicodeNames/UnicodeLayerNames.psb";

	{
		LayeredFile<bpp8_t> layeredFile = LayeredFile<bpp8_t>::read(psb_path);
		LayeredFile<bpp8_t>::write(std::move(layeredFile), psb_path);
	}
	LayeredFile<bpp8_t> layeredFile = LayeredFile<bpp8_t>::read(psb_path);


	// Find the three layers by their names and check if the result is not null
	SUBCASE("Find chinese simplified layer")
	{
		auto ptr = layeredFile.find_layer("Chinese_Simplified/请问可以修改psd 的画板尺寸吗");
		CHECK(ptr);
	}
	SUBCASE("Find overflow layer")
	{
		auto ptr = layeredFile.find_layer("äüöUnicodeNameOverflowPascalString--------------------------------------------------------------------------------------------------------------------");
		CHECK(ptr);
	}
	SUBCASE("Find unicode layer")
	{
		auto ptr = layeredFile.find_layer("UnicodeNameäää");
		CHECK(ptr);
	}
}
