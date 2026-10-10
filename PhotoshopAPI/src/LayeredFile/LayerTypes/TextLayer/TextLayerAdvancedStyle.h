#pragma once

#include "Macros.h"
#include "Core/Struct/EngineDataStructure.h"
#include "Core/TaggedBlocks/TypeToolTaggedBlock.h"
#include <optional>
#include <vector>

PSAPI_NAMESPACE_BEGIN

enum class TextAutoKern : int32_t { Manual = 0, Metrics = 1, Optical = 2 };

/// Layer-wide overrides. Unspecified properties are preserved, including on
/// Photoshop-authored mixed-style text. old_style selects FigureStyle 2/0;
/// it is deliberately NOT the unrelated boolean field OldStyle (22).
struct AdvancedTextStyle
{
    std::optional<bool> stylistic_alternates;
    std::optional<bool> old_style;
    std::optional<TextAutoKern> auto_kern;
    std::optional<bool> fractions;
    std::optional<bool> ordinals;
    std::optional<bool> swash;
    std::optional<bool> titling;
    std::optional<bool> ornaments;
    std::optional<bool> slashed_zero;
    std::optional<bool> contextual_ligatures;

    EngineData::Value features() const;
};

namespace TextLayerDetail
{
    struct AdvancedTextInput
    {
        std::shared_ptr<TypeToolTaggedBlock> tysh;
        EngineData::Value overrides = EngineData::make_dict();
        bool newly_created = false;
        bool point_text = false;
        bool vertical = false;
    };

    // Called before layer records are serialized. Untouched documents take the
    // identity path, preserving both Txt2 bytes and existing image caches.
    std::vector<std::shared_ptr<TaggedBlock>> prepare_advanced_text(
        const std::vector<AdvancedTextInput>& layers,
        const std::vector<std::shared_ptr<TaggedBlock>>& original_blocks);
}

PSAPI_NAMESPACE_END
