#include "ProtectedSettingTaggedBlock.h"

#include "Core/FileIO/LengthMarkers.h"

PSAPI_NAMESPACE_BEGIN


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
void ProtectedSettingTaggedBlock::read(File& document, const uint64_t offset, const Signature signature)
{
	m_Key = Enum::TaggedBlockKey::lrProtectedSetting;
	m_Offset = offset;
	m_Signature = signature;
	uint32_t length = ReadBinaryData<uint32_t>(document);
	m_Length = length;

	if (length != 4u)
	{
		PSAPI_LOG_WARNING("ProtectedSettingTaggedBlock", "Block size did not match expected size of 4, instead got %d, skipping reading this block", length);
		document.setOffset(offset + 4u + length);
		return;
	}

	m_ProtectionFlags = ReadBinaryData<uint32_t>(document);
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
void ProtectedSettingTaggedBlock::write(File& document, [[maybe_unused]] const FileHeader& header, [[maybe_unused]] ProgressCallback& callback, [[maybe_unused]] const uint16_t padding /*= 1u*/)
{
	WriteBinaryData<uint32_t>(document, Signature("8BIM").m_Value);
	WriteBinaryData<uint32_t>(document, Signature("lspf").m_Value);

	Impl::ScopedLengthBlock<uint32_t> len_block(document, padding);

	WriteBinaryData<uint32_t>(document, m_ProtectionFlags);
}


PSAPI_NAMESPACE_END