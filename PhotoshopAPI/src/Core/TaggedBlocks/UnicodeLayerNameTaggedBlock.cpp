#include "UnicodeLayerNameTaggedBlock.h"

#include "Core/FileIO/LengthMarkers.h"
#include "Core/FileIO/Util.h"
#include "Core/FileIO/Write.h"

PSAPI_NAMESPACE_BEGIN


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
void UnicodeLayerNameTaggedBlock::read(File& document, const uint64_t offset, const Signature signature, const uint16_t padding /*= 1u*/)
{
	m_Key = Enum::TaggedBlockKey::lrUnicodeName;
	m_Offset = offset;
	m_Signature = signature;
	uint32_t length = ReadBinaryData<uint32_t>(document);
	length = RoundUpToMultiple<uint32_t>(length, padding);
	m_Length = length;

	auto start_offset = document.get_offset();

	m_Name.read(document, 4u);

	document.set_offset(start_offset + length);
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
void UnicodeLayerNameTaggedBlock::write(File& document, [[maybe_unused]] const FileHeader& header, [[maybe_unused]] ProgressCallback& callback, const uint16_t padding /*= 1u*/)
{
	WriteBinaryData<uint32_t>(document, Signature("8BIM").m_Value);
	WriteBinaryData<uint32_t>(document, Signature("luni").m_Value);
	Impl::ScopedLengthBlock<uint32_t> len_block(document, padding);

	auto utf16_string = m_Name.getUTF16String();
	while (!utf16_string.empty() && utf16_string.back() == static_cast<char16_t>(0))
	{
		utf16_string.pop_back();
	}

	std::vector<uint16_t> string_data;
	string_data.reserve(utf16_string.size());
	for (const auto code_unit : utf16_string)
	{
		string_data.push_back(static_cast<uint16_t>(code_unit));
	}

	const auto utf16strlen = string_data.size();
	WriteBinaryData<uint32_t>(document, static_cast<uint32_t>(utf16strlen));
	WriteBinaryArray<uint16_t>(document, std::move(string_data));

	const auto byte_size = utf16strlen * sizeof(uint16_t) + sizeof(uint32_t);
	const auto pad_size = RoundUpToMultiple<uint64_t>(byte_size, padding) - byte_size;
	WritePadddingBytes(document, pad_size);
}


PSAPI_NAMESPACE_END
