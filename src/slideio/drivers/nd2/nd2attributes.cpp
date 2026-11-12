// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "slideio/drivers/nd2/nd2internal.hpp"

#include <cstdio>
#include <limits>

using namespace slideio;
using namespace slideio::nd2;
using slideio::nd2::detail::elementsOf;
using slideio::nd2::detail::fieldOf;

namespace
{
    [[noreturn]] void refuse(const std::string& field, const std::string& why)
    {
        throw Unsupported("ND2: unsupported image attributes: " + field + " " + why);
    }

    /// A positive value that fits an int, or Unsupported naming the field.
    int positive(const Variant& attrs, const char* field)
    {
        const Variant* v = attrs.get(field);
        const int64_t value = v ? v->asInt(0) : 0;
        if (value <= 0 || value > std::numeric_limits<int>::max()) {
            refuse(field, "is " + std::to_string(value) + " (expected 1.." +
                std::to_string(std::numeric_limits<int>::max()) + ")");
        }
        return static_cast<int>(value);
    }

    void checkTile(const Variant& attrs, const char* field, int frameSize)
    {
        const Variant* v = attrs.get(field);
        if (!v) {
            return;
        }
        const int64_t tile = v->asInt(0);
        // 0 is how a writer says "no tiling"; anything else must be the whole frame.
        if (tile != 0 && tile != frameSize) {
            refuse(field, "is " + std::to_string(tile) + " but the frame is " +
                std::to_string(frameSize) + " (tiled frames are not supported)");
        }
    }
}

DataType Attributes::dataType() const
{
    switch (bitsInMemory) {
    case 8: return DataType::DT_Byte;
    case 16: return DataType::DT_UInt16;
    case 32: return DataType::DT_Float32;
    default: return DataType::DT_Unknown;
    }
}

Attributes Attributes::parse(const Variant& attrs)
{
    if (attrs.kind() != Variant::Kind::Map) {
        throw Unsupported("ND2: unsupported image attributes: the attributes record is missing");
    }
    Attributes a;
    a.width = positive(attrs, "uiWidth");
    a.height = positive(attrs, "uiHeight");
    a.components = positive(attrs, "uiComp");
    const Variant* bits = attrs.get("uiBpcInMemory");
    const int64_t bitsValue = bits ? bits->asInt(0) : 0;
    if (bitsValue != 8 && bitsValue != 16 && bitsValue != 32) {
        refuse("uiBpcInMemory", "is " + std::to_string(bitsValue) + " (expected 8, 16 or 32)");
    }
    a.bitsInMemory = static_cast<int>(bitsValue);

    const Variant* sig = attrs.get("uiBpcSignificant");
    const int64_t sigValue = sig ? sig->asInt(0) : 0;
    a.significantBits = (sigValue > 0 && sigValue <= std::numeric_limits<int>::max())
        ? static_cast<int>(sigValue) : a.bitsInMemory;

    const Variant* stride = attrs.get("uiWidthBytes");
    a.rowStride = stride ? stride->asInt(0) : 0;
    const int64_t packed = static_cast<int64_t>(a.width) * a.components * a.sampleBytes();
    if (a.rowStride < packed) {
        refuse("uiWidthBytes", "is " + std::to_string(a.rowStride) + " but a packed row needs " +
            std::to_string(packed));
    }

    const Variant* seq = attrs.get("uiSequenceCount");
    a.sequenceCount = seq ? seq->asInt(0) : 0;

    // 0 = lossless (zlib), 1 = lossy, 2 = none; missing = none (FD 5).
    const Variant* comp = attrs.get("eCompression");
    const int64_t compValue = comp ? comp->asInt(2) : 2;
    if (compValue == 0) {
        a.compression = FrameCompression::Zlib;
    }
    else if (compValue == 2) {
        a.compression = FrameCompression::None;
    }
    else {
        refuse("eCompression", "is " + std::to_string(compValue) +
            (compValue == 1 ? " (lossy compression is not supported)" : " (unknown compression)"));
    }

    checkTile(attrs, "uiTileWidth", a.width);
    checkTile(attrs, "uiTileHeight", a.height);
    return a;
}

namespace
{
    /// `sPicturePlanes.sPlaneNew`, else `sPlane`: the plane records.
    std::vector<const Variant*> planeRecords(const Variant* planes)
    {
        const Variant* records = fieldOf(planes, "sPlaneNew");
        if (!records) {
            records = fieldOf(planes, "sPlane");
        }
        return elementsOf(records);
    }
}

int64_t slideio::nd2::detail::pictureChannelCount(const Variant* pictureMetadata)
{
    const Variant* planes = fieldOf(pictureMetadata, "sPicturePlanes");
    const Variant* countField = fieldOf(planes, "uiCount");
    const int64_t count = countField ? countField->asInt(0) : 0;
    return count > 0 ? count : static_cast<int64_t>(planeRecords(planes).size());
}

std::vector<ChannelInfo> slideio::nd2::parseChannels(const Variant* pictureMetadata, int components,
    int64_t channelsWithoutMetadata)
{
    if (components <= 0) {
        throw Unsupported("ND2: unsupported image attributes: uiComp is " + std::to_string(components));
    }
    const std::vector<const Variant*> plane = planeRecords(fieldOf(pictureMetadata, "sPicturePlanes"));
    int64_t channelCount = detail::pictureChannelCount(pictureMetadata);
    if (channelCount <= 0) {
        channelCount = channelsWithoutMetadata;
    }

    std::vector<ChannelInfo> out;
    out.reserve(static_cast<size_t>(components));
    if (channelCount <= 0) {
        // No picture metadata: every component is a single-component channel.
        for (int k = 0; k < components; ++k) {
            ChannelInfo ch;
            ch.nd2Channel = k;
            out.push_back(ch);
        }
        return out;
    }
    if (components % channelCount != 0) {
        throw Unsupported("ND2: unsupported channel layout: uiComp " + std::to_string(components) +
            " is not a multiple of sPicturePlanes.uiCount " + std::to_string(channelCount));
    }
    const int perChannel = static_cast<int>(components / channelCount);
    static const char* const rgb[] = { "R", "G", "B" };
    for (int k = 0; k < components; ++k) {
        const int c = k / perChannel;
        const int j = k % perChannel;
        ChannelInfo ch;
        ch.nd2Channel = c;
        ch.component = j;
        std::string base;
        if (static_cast<size_t>(c) < plane.size()) {
            const Variant& rec = *plane[static_cast<size_t>(c)];
            if (const Variant* name = fieldOf(&rec, "sDescription")) {
                base = name->asString();
            }
            if (const Variant* color = fieldOf(&rec, "uiColor")) {
                ch.color = static_cast<uint32_t>(color->asInt(0xFFFFFF));
            }
            if (const Variant* probe = fieldOf(fieldOf(&rec, "pFluorescentProbe"), "m_sName")) {
                const std::string text = probe->asString();
                if (!text.empty()) {
                    ch.attributes.emplace_back("FluorescentProbe", text);
                }
            }
        }
        if (perChannel == 1) {
            ch.name = base;
        }
        else {
            const std::string suffix = perChannel == 3 ? rgb[j] : std::to_string(j);
            ch.name = base.empty() ? suffix : base + " " + suffix;
        }
        out.push_back(std::move(ch));
    }
    return out;
}

std::string slideio::nd2::colorToHex(uint32_t packedBGR)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
        static_cast<unsigned>(packedBGR & 0xFF),
        static_cast<unsigned>((packedBGR >> 8) & 0xFF),
        static_cast<unsigned>((packedBGR >> 16) & 0xFF));
    return buf;
}
