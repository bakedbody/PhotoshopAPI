#pragma once

#include "Macros.h"
#include "TaggedBlock.h"

#include "Core/Struct/File.h"
#include "Core/Struct/Signature.h"
#include "Util/ProgressCallback.h"
#include "Util/Enum.h"
#include "Util/Logger.h"

PSAPI_NAMESPACE_BEGIN


/// Full 32-bit lspf protection mask. Unrecognized bits are retained on write.
struct ProtectedSettingTaggedBlock : TaggedBlock
{
	static constexpr uint32_t Transparency = 0x00000001u;
	static constexpr uint32_t Pixels = 0x00000002u;
	static constexpr uint32_t Position = 0x00000004u;
	static constexpr uint32_t All = 0x80000000u;

	uint32_t m_ProtectionFlags = 0u;

	ProtectedSettingTaggedBlock() = default;
	explicit ProtectedSettingTaggedBlock(uint32_t flags) : m_ProtectionFlags(flags)
	{
		m_Key = Enum::TaggedBlockKey::lrProtectedSetting;
	}
	ProtectedSettingTaggedBlock(bool isLocked) : ProtectedSettingTaggedBlock(isLocked ? All : 0u) {}

	void read(File& document, const uint64_t offset, const Signature signature);
	void write(File& document, const FileHeader& header, ProgressCallback& callback, const uint16_t padding = 1u) override;
};


PSAPI_NAMESPACE_END