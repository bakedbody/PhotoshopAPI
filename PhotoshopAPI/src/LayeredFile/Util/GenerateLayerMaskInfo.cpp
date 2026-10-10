
#include "GenerateLayerMaskInfo.h"

#include "Macros.h"
#include "Core/TaggedBlocks/TaggedBlock.h"
#include "LayeredFile/LayerTypes/Layer.h"
#include "LayeredFile/LayerTypes/TextLayer/TextLayer.h"

#include <variant>
#include <optional>
#include <memory>
#include <algorithm>


PSAPI_NAMESPACE_BEGIN

template <typename T>
static auto prepare_text_blocks(LayeredFile<T>& file)
{
	std::vector<TextLayerDetail::AdvancedTextInput> inputs;
	for (auto& layer : file.flat_layers())
		if (auto text = std::dynamic_pointer_cast<TextLayer<T>>(layer))
			inputs.push_back(text->internal_advanced_text_input());
	if (file.colormode() != Enum::ColorMode::RGB &&
		std::any_of(inputs.begin(), inputs.end(), [](const auto& i) { return !i.overrides.dictionary_items.empty(); }))
		throw std::invalid_argument("Native advanced text currently supports RGB documents only");
	return TextLayerDetail::prepare_advanced_text(inputs, file.unparsed_blocks());
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template <typename T>
LayerAndMaskInformation generate_layermaskinfo(LayeredFile<T>& layeredFile, std::filesystem::path file_path)
{
	PSAPI_LOG_ERROR("LayeredFile", "Cannot construct layer and mask information section if type is not uint8_t, uint16_t or float32_t");
	return LayerAndMaskInformation();
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template <>
LayerAndMaskInformation generate_layermaskinfo(LayeredFile<uint8_t>& layeredFile,std::filesystem::path file_path)
{
	auto unparsed_blocks = prepare_text_blocks(layeredFile);
	LayerInfo lrInfo = generate_layerinfo<uint8_t>(layeredFile);
	// This section is mainly there for backwards compatibility it seems and from initial testing
	// does not appear to really be relevant for documents
	GlobalLayerMaskInfo maskInfo{};

	std::optional<AdditionalLayerInfo> additional_layer_info = std::nullopt;
	std::vector<std::shared_ptr<TaggedBlock>> block_ptrs{};
	if (!layeredFile.linked_layers()->empty() || !unparsed_blocks.empty())
	{
		auto linked_layer_blocks = layeredFile.linked_layers()->to_photoshop(true, file_path);
		for (const auto& block : linked_layer_blocks)
		{
			block_ptrs.push_back(block);
		}
		for (auto& block : unparsed_blocks)
		{
			block_ptrs.push_back(block);
		}		

		AdditionalLayerInfo _additional_info;
		_additional_info.m_TaggedBlocks = TaggedBlockStorage{ block_ptrs };
		additional_layer_info.emplace(std::move(_additional_info));
	}

	return LayerAndMaskInformation(lrInfo, maskInfo, std::move(additional_layer_info));
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template <>
LayerAndMaskInformation generate_layermaskinfo(LayeredFile<uint16_t>& layeredFile, std::filesystem::path file_path)
{
	auto unparsed_blocks = prepare_text_blocks(layeredFile);
	LayerInfo emptyLrInfo{};
	LayerInfo lrInfo = generate_layerinfo<uint16_t>(layeredFile);
	// This section is mainly there for backwards compatibility it seems and from initial testing
	// does not appear to really be relevant for documents
	GlobalLayerMaskInfo maskInfo{};

	std::vector<std::shared_ptr<TaggedBlock>> block_ptrs{};
	block_ptrs.push_back(std::make_shared<Lr16TaggedBlock>(lrInfo));

	if (!layeredFile.linked_layers()->empty())
	{
		auto linked_layer_blocks = layeredFile.linked_layers()->to_photoshop(true, file_path);
		for (const auto& block : linked_layer_blocks)
		{
			block_ptrs.push_back(block);
		}
	}
	for (auto& block : unparsed_blocks)
	{
		block_ptrs.push_back(block);
	}

	TaggedBlockStorage blockStorage(block_ptrs);

	return LayerAndMaskInformation(emptyLrInfo, maskInfo, std::make_optional<AdditionalLayerInfo>(blockStorage));
	
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template <>
LayerAndMaskInformation generate_layermaskinfo(LayeredFile<float32_t>& layeredFile, std::filesystem::path file_path)
{
	auto unparsed_blocks = prepare_text_blocks(layeredFile);
	LayerInfo emptyLrInfo{};
	LayerInfo lrInfo = generate_layerinfo<float32_t>(layeredFile);
	// This section is mainly there for backwards compatibility it seems and from initial testing
	// does not appear to really be relevant for documents
	GlobalLayerMaskInfo maskInfo{};

	std::vector<std::shared_ptr<TaggedBlock>> block_ptrs{};
	block_ptrs.push_back(std::make_shared<Lr32TaggedBlock>(lrInfo));

	if (!layeredFile.linked_layers()->empty())
	{
		auto linked_layer_blocks = layeredFile.linked_layers()->to_photoshop(true, file_path);
		for (const auto& block : linked_layer_blocks)
		{
			block_ptrs.push_back(block);
		}
	}
	for (auto& block : unparsed_blocks)
	{
		block_ptrs.push_back(block);
	}

	TaggedBlockStorage blockStorage(block_ptrs);

	return LayerAndMaskInformation(emptyLrInfo, maskInfo, std::make_optional<AdditionalLayerInfo>(blockStorage));
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template <typename T>
LayerInfo generate_layerinfo(LayeredFile<T>& layeredFile)
{
	// We must first for each layer generate a layer records as well as channelImageData using the reversed flat layers
	std::vector<std::shared_ptr<Layer<T>>> flatLayers = layeredFile.flat_layers(std::nullopt, LayerOrder::reverse);


	std::vector<LayerRecord> layerRecords;
	std::vector<ChannelImageData> imageData;
	layerRecords.reserve(flatLayers.size());
	imageData.reserve(flatLayers.size());

	for (const auto& layer : flatLayers)
	{
		std::tuple<LayerRecord, ChannelImageData> lrData = generate_layerdata<T>(layer);
		LayerRecord lrRecord = std::move(std::get<0>(lrData));
		ChannelImageData lrImageData = std::move(std::get<1>(lrData));

		layerRecords.push_back(std::move(lrRecord));
		imageData.push_back(std::move(lrImageData));
	}

	LayerInfo info(std::move(layerRecords), std::move(imageData));
	info.m_HasMergedTransparency = layeredFile.has_merged_transparency();
	return info;
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template <typename T>
std::tuple<LayerRecord, ChannelImageData> generate_layerdata(std::shared_ptr<Layer<T>> layer)
{
	auto lrData = layer->to_photoshop();
	return lrData;
}


PSAPI_NAMESPACE_END
