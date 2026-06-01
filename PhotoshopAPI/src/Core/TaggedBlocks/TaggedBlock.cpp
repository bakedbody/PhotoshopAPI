#include "TaggedBlock.h"

#include "Macros.h"
#include "Enum.h"
#include "Logger.h"
#include "StringUtil.h"
#include "Core/Struct/Signature.h"
#include "Core/Struct/File.h"
#include "PhotoshopFile/FileHeader.h"
#include "Core/FileIO/Read.h"
#include "Core/FileIO/Write.h"
#include "Core/FileIO/LengthMarkers.h"

#include <cassert>
#include <ctime>
#include <vector>
#include <span>

PSAPI_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
void TaggedBlock::read(File& document, const FileHeader& header, const uint64_t offset, const Signature signature, const Enum::TaggedBlockKey key, const uint16_t padding /* = 1u */)
{
	m_Offset = offset;
	m_Signature = signature;
	m_Key = key;
	if (Enum::isTaggedBlockSizeUint64(m_Key) && header.m_Version == Enum::Version::Psb)
	{
		uint64_t length = ReadBinaryData<uint64_t>(document);
		m_Length = length;
		m_Data = std::vector<std::byte>(length);
		document.read(std::span<uint8_t>(reinterpret_cast<uint8_t*>(m_Data.data()), m_Data.size()));
		document.skip(RoundUpToMultiple<uint64_t>(length, padding) - length);
	}
	else
	{
		uint32_t length = ReadBinaryData<uint32_t>(document);
		m_Length = length;
		m_Data = std::vector<std::byte>(length);
		document.read(std::span<uint8_t>(reinterpret_cast<uint8_t*>(m_Data.data()), m_Data.size()));
		document.skip(RoundUpToMultiple<uint32_t>(length, padding) - length);
	}
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
void TaggedBlock::write(File& document, [[maybe_unused]] const FileHeader& header, [[maybe_unused]] ProgressCallback& callback, [[maybe_unused]] const uint16_t padding /* = 1u */)
{
	WriteBinaryData<uint32_t>(document, m_Signature.m_Value);
	std::optional<std::vector<std::string>> keyStr = Enum::get_tagged_block_key<Enum::TaggedBlockKey, std::vector<std::string>>(m_Key);
	if (!keyStr.has_value())
	{
		PSAPI_LOG_ERROR("TaggedBlock", "Was unable to extract a string from the tagged block key");
	}
	else
	{
		// We use the first found value from the key matches
		WriteBinaryData<uint32_t>(document, Signature(keyStr.value()[0]).m_Value);
	}

	if (isTaggedBlockSizeUint64(m_Key) && header.m_Version == Enum::Version::Psb)
	{
		WriteBinaryData<uint64_t>(document, static_cast<uint64_t>(m_Data.size()));
		WriteBinaryArray<std::byte>(document, m_Data);
		WritePadddingBytes(document, RoundUpToMultiple<uint64_t>(static_cast<uint64_t>(m_Data.size()), padding) - m_Data.size());
	}
	else
	{
		WriteBinaryData<uint32_t>(document, static_cast<uint32_t>(m_Data.size()));
		WriteBinaryArray<std::byte>(document, m_Data);
		WritePadddingBytes(document, RoundUpToMultiple<uint32_t>(static_cast<uint32_t>(m_Data.size()), padding) - m_Data.size());
	}
}

PSAPI_NAMESPACE_END
