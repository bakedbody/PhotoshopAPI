#include "TextLayerAdvancedStyle.h"
#include "TextLayerEngineDataUtils.h"
#include "TextLayerTyShBuilderUtils.h"
#include <algorithm>
#include <stdexcept>

PSAPI_NAMESPACE_BEGIN

EngineData::Value AdvancedTextStyle::features() const
{
    using namespace EngineData;
    auto result = make_dict();
    const auto put = [&](const char* key, std::optional<bool> value) {
        if (value) insert_dict_value(result, key, make_bool(*value));
    };
    put("28", stylistic_alternates);
    put("23", fractions); put("24", ordinals); put("25", swash);
    put("26", titling); put("29", ornaments); put("84", slashed_zero);
    put("20", contextual_ligatures);
    if (old_style) insert_dict_value(result, "30", make_integer(*old_style ? 2 : 0));
    if (auto_kern)
    {
        const auto value = static_cast<int32_t>(*auto_kern);
        if (value < 0 || value > 2) throw std::invalid_argument("Invalid TextAutoKern");
        insert_dict_value(result, "11", make_integer(value));
    }
    return result;
}

namespace TextLayerDetail
{
namespace
{
using namespace EngineData;
using V = Value;

V dict(std::initializer_list<V::DictionaryItem> values)
{
    auto v = make_dict();
    v.dictionary_items = values;
    return v;
}
V array(std::initializer_list<V> values)
{
    auto v = make_array();
    v.array_items = values;
    return v;
}
V string(const std::string& value)
{
    // The existing UTF conversion helper warns for an empty string. An empty
    // EngineData Unicode literal still requires its UTF-16BE BOM.
    return make_string(value.empty() ? std::string("\xfe\xff", 2) : encode_engine_literal_utf16be(value));
}
V& at(V& v, const char* key)
{
    auto* found = find_dict_value(v, key);
    if (!found) throw std::runtime_error(std::string("Unsupported Txt2 structure: missing /") + key);
    return *found;
}
V& index(V& v, size_t i)
{
    if (v.type != ValueType::Array || i >= v.array_items.size())
        throw std::runtime_error("Unsupported Txt2 array/index");
    return v.array_items[i];
}
V parse_ascii(const std::string& s)
{
    const auto* start = reinterpret_cast<const std::byte*>(s.data());
    auto parsed = parse(std::vector<std::byte>(start, start + s.size()));
    if (!parsed.ok) throw std::runtime_error("Invalid native text defaults: " + parsed.error_message);
    return parsed.root;
}
V paint(const V& type, const V& values)
{
    return dict({{"99", make_name("SimplePaint")}, {"0", dict({{"0", type}, {"1", values}})}});
}
V char_defaults()
{
    // Explicit semantic defaults, not a captured Photoshop payload. Numeric
    // keys are scoped to CharacterStyle (Photoshop 27.6 schema).
    auto v = parse_ascii(R"(<<
        /1 12.0 /6 1.0 /7 1.0 /11 1 /35 2 /37 2
        /41 2 /43 << /0 0.5 >> /44 2 /45 2 /46 7 /49 -1.0 /50 -1.0
        /55 << /99 /SimpleBlender >> /63 1.0 /64 4.0 /66 [] /67 [] /71 4
        /82 3.0 /83 3.0 /86 << /99 /SimpleCustomFeature >>
        /87 100.0 /95 << /0 0.0 /1 0 >> >>)");
    for (int k : {2,3,19,20,21,22,23,24,25,26,27,28,29,31,32,33,34,40,52,57,59,60,74,75,76,80,84})
        insert_dict_value(v, std::to_string(k), make_bool(false));
    for (int k : {4,18,56,58,77,78,88,96})
        insert_dict_value(v, std::to_string(k), make_bool(true));
    for (int k : {0,8,12,13,14,15,16,30,38,39,42,47,48,51,61,62,68,69,70,81,85,91})
        insert_dict_value(v, std::to_string(k), make_integer(0));
    for (int k : {5,9,10,17,36,65,72,73,92})
        insert_dict_value(v, std::to_string(k), make_float(0.0));
    auto black = parse_ascii("[1.0 0.0 0.0 0.0]");
    insert_dict_value(v, "53", paint(make_integer(1), black));
    insert_dict_value(v, "54", paint(make_integer(1), black));
    insert_dict_value(v, "79", paint(make_integer(1), parse_ascii("[1.0 1.0 1.0 0.0]")));
    return v;
}
V para_defaults()
{
    return parse_ascii(R"(<<
        /0 0 /1 0.0 /2 0.0 /3 0.0 /4 0.0 /5 0.0 /6 1 /7 1.2 /8 0
        /9 true /10 6 /11 2 /12 2 /13 0 /14 36.0 /15 true /16 0.5
        /17 [0.8 1.0 1.33] /18 [0.0 0.0 0.0] /19 [1.0 1.0 1.0]
        /20 6 /21 false /22 0 /23 true /24 0 /25 0 /26 false
        /27 /nil /28 /nil /29 false /30 <<>> /31 36.0 /32 <<>>
        /33 0 /34 7 /35 1 /36 /nil /37 0 /38 false /39 0 /40 2 /41 0
        /47 false /48 /nil >>)");
}
V resource_set(V object)
{
    return dict({{"0", array({dict({{"0", std::move(object)}})})},
                 {"1", array({dict({{"0", make_integer(0)}})})}});
}
V empty_document()
{
    auto c = char_defaults(), p = para_defaults();
    auto resources = dict({
        {"1", dict({{"0", make_array()}})},
        {"5", resource_set(dict({{"0", string("Normal RGB")}, {"6", c}}))},
        {"6", resource_set(dict({{"0", string("Normal Paragraph")}, {"5", p}}))},
        {"8", dict({{"0", make_array()}})} });
    auto document = dict({
        {"4", dict({{"0", make_integer(7)}, {"1", make_integer(11)},
                    {"2", make_integer(0)}, {"3", string("Photoshop")}})},
        {"0", dict({{"1", make_integer(0)}, {"2", make_integer(0)}})},
        {"1", make_array()}, {"2", c}, {"3", p} });
    return dict({{"98", dict({{"0", make_integer(14)}})}, {"0", resources}, {"1", document}});
}

// Only mappings with the same value representation are copied directly.
struct Mapping { const char* name; const char* id; };
constexpr Mapping char_map[] = {
    {"FontSize","1"}, {"FauxBold","2"}, {"FauxItalic","3"}, {"AutoLeading","4"},
    {"Leading","5"}, {"HorizontalScale","6"}, {"VerticalScale","7"}, {"Tracking","8"},
    {"BaselineShift","9"}, {"FontCaps","12"}, {"FontBaseline","13"},
    {"Ligatures","18"}, {"DLigatures","19"}, {"BaselineDirection","35"}, {"Tsume","36"},
    {"StyleRunAlignment","37"}, {"Language","38"}, {"NoBreak","52"},
    {"FillFlag","56"}, {"StrokeFlag","57"}, {"FillFirst","58"}, {"OutlineWidth","63"}
};
constexpr Mapping para_map[] = {
    {"Justification","0"}, {"FirstLineIndent","1"}, {"StartIndent","2"}, {"EndIndent","3"},
    {"SpaceBefore","4"}, {"SpaceAfter","5"}, {"AutoLeading","7"}, {"LeadingType","8"},
    {"AutoHyphenate","9"}, {"HyphenatedWordSize","10"}, {"PreHyphen","11"}, {"PostHyphen","12"},
    {"ConsecutiveHyphens","13"}, {"Zone","14"}, {"WordSpacing","17"},
    {"LetterSpacing","18"}, {"GlyphSpacing","19"}
};
template<size_t N>
void copy_fields(V& to, const V& from, const Mapping (&mapping)[N])
{
    for (const auto& field : mapping)
        if (auto* v = find_dict_value(from, field.name)) insert_dict_value(to, field.id, *v);
}
void overlay(V& to, const V& from)
{
    for (const auto& entry : from.dictionary_items) insert_dict_value(to, entry.first, entry.second);
}

int32_t text_index(TypeToolTaggedBlock& tysh)
{
    if (!tysh.has_parsed_descriptors() && !tysh.parse_descriptors_from_data())
        throw std::runtime_error("Unable to parse TySh for advanced text");
    if (!tysh.text_descriptor().contains("TextIndex")) return -1;
    return tysh.text_descriptor().at<int32_t>("TextIndex");
}
void set_index(TypeToolTaggedBlock& tysh, int32_t value, bool update)
{
    tysh.text_descriptor().insert_or_assign("TextIndex",
        std::make_unique<Descriptors::int32_t_Wrapper>(value));
    if (update)
        tysh.text_descriptor().insert_or_assign("oldTextAge",
            std::make_unique<Descriptors::Enumerated>("oldTextAge",
                std::vector<char>{'e','n','u','m'}, "oldTextAge", "ate1"));
    tysh.sync_data_from_descriptors();
}

V make_object(V& tree, const AdvancedTextInput& layer)
{
    if (!layer.point_text)
        throw std::invalid_argument("Native Txt2 generation requires point text; existing Txt2 can be edited without rebuilding frames");
    const auto payload = read_engine_payload(*layer.tysh);
    if (!payload) throw std::runtime_error("Missing TySh EngineData");
    auto parsed = parse(*payload);
    if (!parsed.ok) throw std::runtime_error("Invalid TySh EngineData");
    auto& engine = parsed.root;
    auto& resources = at(tree, "0");
    auto& document = at(tree, "1");
    auto& fonts = at(at(resources, "1"), "0");
    auto& source_fonts = at(at(engine, "ResourceDict"), "FontSet");
    const auto font_offset = static_cast<int64_t>(fonts.array_items.size());
    for (auto& font : source_fonts.array_items)
    {
        auto descriptor = dict({{"0", at(font, "Name")}, {"1", make_integer(3)}, {"2", make_integer(1)}});
        auto native_font = dict({{"99", make_name("CoolTypeFont")}, {"0", descriptor}});
        append_array_item(fonts, dict({{"0", native_font}}));
    }
    auto& ed = at(engine, "EngineDict");
    auto native_runs = [&](bool character) {
        auto& src = at(ed, character ? "StyleRun" : "ParagraphRun");
        auto& runs = at(src, "RunArray");
        auto& lengths = at(src, "RunLengthArray");
        auto result = make_array();
        for (size_t i = 0; i < runs.array_items.size(); ++i)
        {
            auto features = make_dict();
            auto& old_sheet = at(index(runs, i), character ? "StyleSheet" : "ParagraphSheet");
            auto& props = at(old_sheet, character ? "StyleSheetData" : "Properties");
            if (character)
            {
                if (auto* kern = find_dict_value(props, "Kerning"); kern && kern->number_value != 0)
                    throw std::invalid_argument("Native Txt2 generation does not yet map nonzero pair kerning");
                copy_fields(features, props, char_map);
                const auto* font = find_dict_value(props, "Font");
                const auto local_font = font ? static_cast<int64_t>(font->number_value) : 0;
                if (local_font < 0 || static_cast<size_t>(local_font) >= source_fonts.array_items.size())
                    throw std::invalid_argument("Text run references an invalid font");
                insert_dict_value(features, "0", make_integer(font_offset + local_font));
                if (auto* kern = find_dict_value(props, "AutoKerning"))
                    insert_dict_value(features, "11", make_integer(kern->bool_value ? 1 : 0));
                for (const auto& color : {Mapping{"FillColor","53"}, Mapping{"StrokeColor","54"}})
                    if (auto* c = find_dict_value(props, color.name))
                        insert_dict_value(features, color.id, paint(at(*c,"Type"), at(*c,"Values")));
                for (const auto& line : {Mapping{"Underline","16"}, Mapping{"Strikethrough","15"}})
                    if (auto* b = find_dict_value(props, line.name))
                        insert_dict_value(features, line.id, make_integer(b->bool_value ? 1 : 0));
                overlay(features, layer.overrides);
            }
            else copy_fields(features, props, para_map);
            auto& settings = at(document, "0");
            auto sheet = character
                ? dict({{"0", string("")}, {"5", at(settings,"1")}, {"6", features}})
                : dict({{"0", string("")}, {"5", features}, {"6", at(settings,"2")}});
            append_array_item(result, dict({{"0", dict({{"0", sheet}})}, {"1", index(lengths, i)}}));
        }
        return dict({{"0", result}});
    };
    auto model = dict({{"0", at(at(ed,"Editor"),"Text")},
                       {"5", native_runs(false)}, {"6", native_runs(true)}});
    auto& frames = at(at(resources,"8"),"0");
    const auto frame_index = static_cast<int64_t>(frames.array_items.size());
    // A valid frame reference is mandatory even with no rendered glyph caches.
    auto frame = parse_ascii("<< /0 << /2 << /6 [-1.0 -1.0] /11 << /4 -1 /18 -1.0 /22 0.025 >> >> >> >>");
    // The native text engine takes orientation from its frame, not TySh/Ornt.
    // Field 1 in the frame's /2 substructure is 2 for vertical; omitted = horizontal.
    if (layer.vertical) insert_dict_value(at(at(frame,"0"),"2"), "1", make_integer(2));
    append_array_item(frames, frame);
    auto view = dict({{"0", array({dict({{"0", make_integer(frame_index)}})})}});
    return dict({{"0", model}, {"1", view}});
}

struct NativeTxt2Block : TaggedBlock
{
    explicit NativeTxt2Block(const V& tree)
    {
        m_Key = Enum::TaggedBlockKey::lrTextEngineData;
        // Txt2 has unwrapped root dictionary entries (unlike TySh EngineData).
        for (const auto& entry : tree.dictionary_items)
        {
            const auto key = "/" + entry.first + " ";
            for (char c : key) m_Data.push_back(static_cast<std::byte>(c));
            auto value = format_value_bytes(entry.second);
            m_Data.insert(m_Data.end(), value.begin(), value.end());
            m_Data.push_back(std::byte{'\n'});
        }
    }
};
}

std::vector<std::shared_ptr<TaggedBlock>> prepare_advanced_text(
    const std::vector<AdvancedTextInput>& layers,
    const std::vector<std::shared_ptr<TaggedBlock>>& original_blocks)
{
    if (std::none_of(layers.begin(), layers.end(), [](const auto& l) { return !l.overrides.dictionary_items.empty(); }))
        return original_blocks;

    auto tree = empty_document();
    bool has_existing = false;
    for (auto& block : original_blocks)
    {
        if (!block || block->getKey() != Enum::TaggedBlockKey::lrTextEngineData) continue;
        if (has_existing) throw std::runtime_error("Multiple Txt2 blocks are not supported for editing");
        std::vector<std::byte> wrapped{std::byte{'<'}, std::byte{'<'}, std::byte{'\n'}};
        wrapped.insert(wrapped.end(), block->m_Data.begin(), block->m_Data.end());
        // Some writers pad the payload with null bytes.
        while (!wrapped.empty() && wrapped.back() == std::byte{0}) wrapped.pop_back();
        wrapped.insert(wrapped.end(), {std::byte{'\n'},std::byte{'>'},std::byte{'>'}});
        auto parsed = parse(wrapped);
        if (!parsed.ok) throw std::runtime_error("Cannot edit Txt2: " + parsed.error_message);
        tree = std::move(parsed.root);
        has_existing = true;
    }

    // Finish validation/construction before modifying any TySh block.
    struct Assignment { std::shared_ptr<TypeToolTaggedBlock> block; int32_t id; bool update; };
    std::vector<Assignment> assignments;
    for (const auto& layer : layers)
    {
        if (!layer.tysh) continue;
        const auto old_index = text_index(*layer.tysh);
        if (layer.overrides.dictionary_items.empty())
        {
            // Fresh layers otherwise all carry the builder's placeholder index 1.
            if (!has_existing || layer.newly_created) assignments.push_back({layer.tysh, -1, false});
            continue;
        }
        auto& objects = at(at(tree,"1"),"1");
        int32_t id = old_index;
        if (!has_existing || layer.newly_created || id < 0)
        {
            auto object = make_object(tree, layer);
            id = static_cast<int32_t>(objects.array_items.size());
            append_array_item(objects, std::move(object));
        }
        else
        {
            auto& runs = at(at(at(index(objects, static_cast<size_t>(id)),"0"),"6"),"0");
            if (runs.type != ValueType::Array || runs.array_items.empty())
                throw std::runtime_error("Txt2 has no character runs");
            for (auto& run : runs.array_items)
            {
                auto& sheet = at(at(run,"0"),"0");
                if (!find_dict_value(sheet,"6")) insert_dict_value(sheet,"6",make_dict());
                overlay(at(sheet,"6"),layer.overrides);
            }
        }
        assignments.push_back({layer.tysh, id, true});
    }
    auto generated = std::make_shared<NativeTxt2Block>(tree);
    auto result = original_blocks;
    std::erase_if(result, [](const auto& b) { return b && b->getKey() == Enum::TaggedBlockKey::lrTextEngineData; });
    result.push_back(std::move(generated));
    for (const auto& a : assignments) set_index(*a.block, a.id, a.update);
    return result;
}
}

PSAPI_NAMESPACE_END
