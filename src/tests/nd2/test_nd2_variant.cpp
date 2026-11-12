// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include "slideio/drivers/nd2/nd2variant.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/core/tools/filereader.hpp"

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    using Bytes = std::vector<uint8_t>;

    std::string fixture(const char* name)
    {
        return std::string(ND2_TEST_DATA_DIR) + "/" + name;
    }

    void append(Bytes& b, const Bytes& more) { b.insert(b.end(), more.begin(), more.end()); }

    template <typename T>
    Bytes le(T v)
    {
        Bytes b(sizeof(T));
        std::memcpy(b.data(), &v, sizeof(T));   // the test host is little-endian
        return b;
    }

    Bytes utf16(const std::u16string& s, bool terminate = true)
    {
        Bytes b;
        for (char16_t c : s) append(b, le<uint16_t>(c));
        if (terminate) append(b, le<uint16_t>(0));
        return b;
    }

    /// One CLX Lite record: type, name length (with NUL), UTF-16 name, value bytes.
    Bytes lite(uint8_t type, const std::u16string& name, const Bytes& value)
    {
        Bytes b{ type, static_cast<uint8_t>(name.size() + 1) };
        append(b, utf16(name));
        append(b, value);
        return b;
    }

    /// A level record, with its index table after the children.
    Bytes level(const std::u16string& name, const std::vector<Bytes>& children)
    {
        Bytes head{ 11, static_cast<uint8_t>(name.size() + 1) };
        append(head, utf16(name));
        Bytes body;
        std::vector<uint64_t> offsets;
        uint64_t pos = head.size() + 12;
        for (const Bytes& c : children) {
            offsets.push_back(pos);
            pos += c.size();
            append(body, c);
        }
        Bytes r = head;
        append(r, le<uint32_t>(static_cast<uint32_t>(children.size())));
        append(r, le<uint64_t>(pos));
        append(r, body);
        for (uint64_t o : offsets) append(r, le<uint64_t>(o));
        return r;
    }

    Bytes zlibOf(const Bytes& raw)
    {
        uLongf n = compressBound(static_cast<uLong>(raw.size()));
        Bytes out(n);
        EXPECT_EQ(compress2(out.data(), &n, raw.data(), static_cast<uLong>(raw.size()), 9), Z_OK);
        out.resize(n);
        return out;
    }

    Variant decode(const Bytes& b) { return decodeClxLite(b.data(), b.size()); }

    Bytes toBytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

    std::string messageOf(const std::function<void()>& f)
    {
        try {
            f();
        }
        catch (const std::exception& ex) {
            return ex.what();
        }
        return {};
    }

    /// Restores the global locale on scope exit.
    struct LocaleGuard
    {
        std::locale saved = std::locale();
        ~LocaleGuard() { std::locale::global(saved); }
    };
}

TEST(Nd2Variant, LiteScalars)
{
    Bytes data;
    append(data, lite(1, u"flag", Bytes{ 7 }));                             // non-zero is true
    append(data, lite(2, u"i32", le<int32_t>(-5)));
    append(data, lite(3, u"u32", le<uint32_t>(4000000000u)));
    append(data, lite(4, u"i64", le<int64_t>(-(int64_t(1) << 40))));
    append(data, lite(5, u"u64", le<uint64_t>(uint64_t(1) << 40)));
    append(data, lite(6, u"dbl", le<double>(1.5)));
    append(data, lite(7, u"ptr", le<uint64_t>(0x1234)));
    append(data, lite(8, u"unit", utf16(u"\u00b5m")));
    Bytes ba = le<uint64_t>(3);
    append(ba, Bytes{ 1, 2, 3 });
    append(data, lite(9, u"blob", ba));

    const Variant v = decode(data);
    ASSERT_EQ(v.kind(), Variant::Kind::Map);
    ASSERT_EQ(v.members().size(), 9u);
    EXPECT_EQ(v.members()[0].first, "flag");   // document order
    EXPECT_EQ(v.get("flag")->kind(), Variant::Kind::Bool);
    EXPECT_TRUE(v.get("flag")->asBool());
    EXPECT_EQ(v.get("i32")->asInt(), -5);
    EXPECT_EQ(v.get("u32")->asInt(), 4000000000LL);
    EXPECT_EQ(v.get("i64")->asInt(), -(int64_t(1) << 40));
    EXPECT_EQ(v.get("u64")->kind(), Variant::Kind::UInt);
    EXPECT_EQ(v.get("u64")->asInt(), int64_t(1) << 40);
    EXPECT_DOUBLE_EQ(v.get("dbl")->asDouble(), 1.5);
    EXPECT_EQ(v.get("ptr")->asInt(), 0x1234);
    EXPECT_EQ(v.get("unit")->asString(), "\xC2\xB5m");
    ASSERT_EQ(v.get("blob")->kind(), Variant::Kind::Bytes);
    EXPECT_EQ(v.get("blob")->bytes().size(), 3u);
    EXPECT_EQ(v.get("missing"), nullptr);
    // lenient conversions
    EXPECT_EQ(v.get("blob")->asInt(42), 42);
    EXPECT_EQ(v.get("unit")->asInt(7), 7);
    EXPECT_DOUBLE_EQ(v.get("i32")->asDouble(), -5.0);
    EXPECT_EQ(v.get("i32")->asString(), "-5");
    EXPECT_EQ(v.get("dbl")->asString(), "1.5");
}

TEST(Nd2Variant, LiteLevelAndList)
{
    const Bytes lv = level(u"SLxThing", {
        lite(3, u"", le<uint32_t>(10)),
        lite(3, u"", le<uint32_t>(20)),
        lite(3, u"", le<uint32_t>(30)),
    });
    Bytes data = lv;
    append(data, lite(3, u"after", le<uint32_t>(99)));   // decodes only if the index table was skipped

    const Variant v = decode(data);
    ASSERT_EQ(v.kind(), Variant::Kind::Map);
    ASSERT_EQ(v.members().size(), 2u);
    const Variant* list = v.get("SLxThing");
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->kind(), Variant::Kind::List);
    ASSERT_EQ(list->items().size(), 3u);
    EXPECT_EQ(list->items()[2].asInt(), 30);
    EXPECT_EQ(v.get("after")->asInt(), 99);
}

TEST(Nd2Variant, LiteNestedLevelsAndCopy)
{
    const Bytes data = level(u"outer", { level(u"inner", { lite(3, u"n", le<uint32_t>(5)) }) });
    Variant copy;
    {
        const Variant v = decode(data);
        copy = *v.path({ "outer", "inner" });   // a copy must outlive the tree it came from
    }
    EXPECT_EQ(copy.get("n")->asInt(), 5);
}

TEST(Nd2Variant, LiteCompressed)
{
    const Bytes plain = level(u"SLxPictureMetadata", { lite(3, u"uiCount", le<uint32_t>(2)), lite(8, u"name", utf16(u"DAPI")) });
    Bytes packed{ 76 };
    append(packed, Bytes(9, 0));   // 10 bytes in all precede the zlib stream
    append(packed, zlibOf(plain));

    const Variant a = decode(plain);
    const Variant b = decode(packed);
    EXPECT_EQ(a.toJson(), b.toJson());
    EXPECT_EQ(b.path({ "SLxPictureMetadata", "uiCount" })->asInt(), 2);

    // two more header bytes, as the fixture generator writes them
    Bytes packed12{ 76, 0 };
    append(packed12, le<uint64_t>(plain.size()));
    append(packed12, Bytes{ 0, 0 });
    append(packed12, zlibOf(plain));
    EXPECT_EQ(decode(packed12).toJson(), a.toJson());

    Bytes garbage{ 76 };
    append(garbage, Bytes(9, 0));
    append(garbage, Bytes{ 0x78, 0x9c, 1, 2, 3, 4, 5, 6 });
    EXPECT_THROW(decode(garbage), std::exception);
}

TEST(Nd2Variant, LiteTruncatedThrows)
{
    Bytes data = lite(8, u"description", utf16(u"hello"));
    const Bytes cutInName(data.begin(), data.begin() + 9);   // 2 header bytes + 3.5 name characters
    const std::string msg = messageOf([&] { decode(cutInName); });
    EXPECT_NE(msg.find("ND2: "), std::string::npos) << msg;

    const Bytes cutInValue(data.begin(), data.end() - 3);
    EXPECT_THROW(decode(cutInValue), std::exception);

    Bytes badLen = lite(9, u"b", le<uint64_t>(1000));
    EXPECT_THROW(decode(badLen), std::exception);

    Bytes lv = level(u"x", { lite(3, u"n", le<uint32_t>(1)) });
    lv.resize(lv.size() - 4);   // the index table is cut
    EXPECT_THROW(decode(lv), std::exception);

    EXPECT_THROW(decode(Bytes{ 99, 1, 0, 0 }), std::exception);   // unknown type
}

TEST(Nd2Variant, XmlScalarsAndLists)
{
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><variant version=\"1.0\"><no_name runtype=\"CLxListVariant\">"
        "<uiWidth runtype=\"lx_uint32\" value=\"1392\"/><dCal runtype=\"double\" value=\"0.5\"/>"
        "<iNeg runtype=\"lx_int32\" value=\"-7\"/><bOn runtype=\"bool\" value=\"true\"/>"
        "<wsName runtype=\"CLxStringW\" value=\"Plan &amp; Apo\"/>"
        "<Wavelength runtype=\"double\" value=\"405\"/><Wavelength runtype=\"double\" value=\"488\"/>"
        "</no_name></variant>";
    const Variant v = decodeMetadata(toBytes(xml));
    ASSERT_EQ(v.kind(), Variant::Kind::Map);
    EXPECT_EQ(v.get("uiWidth")->asInt(), 1392);
    EXPECT_DOUBLE_EQ(v.get("dCal")->asDouble(), 0.5);
    EXPECT_EQ(v.get("iNeg")->asInt(), -7);
    EXPECT_TRUE(v.get("bOn")->asBool());
    EXPECT_EQ(v.get("wsName")->asString(), "Plan & Apo");
    ASSERT_EQ(v.get("Wavelength")->kind(), Variant::Kind::List);
    EXPECT_EQ(v.get("Wavelength")->items().size(), 2u);
    EXPECT_DOUBLE_EQ(v.get("Wavelength")->items()[1].asDouble(), 488.0);

    // The decimal point is '.' whatever the process locale.
    bool switched = false;
    LocaleGuard guard;
    for (const char* name : { "de_DE", "de-DE", "German_Germany", "de_DE.UTF-8", "de_DE.utf8" }) {
        try {
            std::locale::global(std::locale(name));
            switched = true;
            break;
        }
        catch (const std::runtime_error&) {
        }
    }
    if (!switched) {
        std::cout << "[  SKIPPED ] no German locale on this machine; locale part not run\n";
    }
    else {
        const Variant german = decodeMetadata(toBytes(xml));
        EXPECT_DOUBLE_EQ(german.get("dCal")->asDouble(), 0.5);
        EXPECT_EQ(german.get("dCal")->asString(), "0.5");
    }
}

TEST(Nd2Variant, XmlControlCharacters)
{
    const std::string xml = std::string(
        "<variant version=\"1.0\"><no_name runtype=\"CLxListVariant\">"
        "<TextInfoItem_5 runtype=\"CLxStringW\" value=\"Synthetic") + "\x01" + "ND2\"/></no_name></variant>";
    const Variant v = decodeMetadata(toBytes(xml));
    ASSERT_NE(v.get("TextInfoItem_5"), nullptr);
    EXPECT_EQ(v.get("TextInfoItem_5")->asString(), "Synthetic ND2");
}

TEST(Nd2Variant, XmlWrappedLoopPars)
{
    const std::string xml =
        "<variant version=\"1.0\"><no_name runtype=\"CLxListVariant\"><eType runtype=\"lx_uint32\" value=\"1\"/>"
        "<uLoopPars runtype=\"CLxListVariant\"><i0000000000 runtype=\"CLxListVariant\">"
        "<uiCount runtype=\"lx_uint32\" value=\"5\"/></i0000000000></uLoopPars></no_name></variant>";
    const Variant v = decodeMetadata(toBytes(xml));
    const Variant* count = v.path({ "uLoopPars", "i0000000000", "uiCount" });
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->asInt(), 5);
    const Variant* pars = v.get("uLoopPars");
    ASSERT_EQ(pars->items().size(), 1u);
    EXPECT_EQ(pars->items()[0].get("uiCount")->asInt(), 5);
    EXPECT_EQ(v.path({ "uLoopPars", "nope", "uiCount" }), nullptr);
}

TEST(Nd2Variant, XmlRejectsBadInput)
{
    EXPECT_THROW(decodeClxXml("<variant><a"), std::exception);
    EXPECT_NE(messageOf([] { decodeClxXml("<other/>"); }).find("ND2: "), std::string::npos);
    EXPECT_THROW(decodeMetadata({}), std::exception);
    // no external entity is fetched or expanded
    const Variant v = decodeClxXml(
        "<!DOCTYPE variant [<!ENTITY x SYSTEM \"file:///etc/passwd\">]><variant><a runtype=\"CLxStringW\" value=\"&x;\"/></variant>");
    (void)v;
}

TEST(Nd2Variant, LiteAndXmlAgreeOnLists)
{
    const Bytes lite1 = level(u"SLxExperiment", {
        lite(3, u"eType", le<uint32_t>(1)),
        level(u"ppNextLevelEx", { level(u"", { lite(3, u"eType", le<uint32_t>(4)) }) }),
    });
    const std::string xml =
        "<variant><no_name runtype=\"CLxListVariant\"><eType runtype=\"lx_uint32\" value=\"1\"/>"
        "<ppNextLevelEx runtype=\"CLxListVariant\"><no_name runtype=\"CLxListVariant\">"
        "<eType runtype=\"lx_uint32\" value=\"4\"/></no_name></ppNextLevelEx></no_name></variant>";
    const Variant a = decodeMetadata(lite1);
    const Variant b = decodeMetadata(toBytes(xml));
    EXPECT_EQ(a.toJson(), b.toJson());
    ASSERT_EQ(a.get("ppNextLevelEx")->items().size(), 1u);
    EXPECT_EQ(a.get("ppNextLevelEx")->items()[0].get("eType")->asInt(), 4);
}

TEST(Nd2Variant, FixtureTreesAgree)
{
    {
        FileReader file(fixture("v3_tpz.nd2"));
        const ChunkFile c = ChunkFile::read(file);
        const ChunkHeader* h = c.find("ImageMetadataSeqLV|0!");
        ASSERT_NE(h, nullptr);
        const Variant v = decodeMetadata(c.readData(file, *h));   // type-76 compressed
        const Variant* count = v.path({ "sPicturePlanes", "uiCount" });
        ASSERT_NE(count, nullptr);
        EXPECT_EQ(count->asInt(), 2);
    }
    {
        FileReader file(fixture("v21_xml.nd2"));
        const ChunkFile c = ChunkFile::read(file);
        const ChunkHeader* h = c.find("ImageMetadataSeq|0!");
        ASSERT_NE(h, nullptr);
        const Variant v = decodeMetadata(c.readData(file, *h));
        const Variant* count = v.path({ "sPicturePlanes", "uiCount" });
        ASSERT_NE(count, nullptr);
        EXPECT_EQ(count->asInt(), 1);
    }
    {
        FileReader file(fixture("v21_xml.nd2"));
        const ChunkFile c = ChunkFile::read(file);
        const Variant text = decodeMetadata(c.readData(file, *c.find("ImageTextInfo!")));   // holds the raw \x01
        EXPECT_NE(text.get("TextInfoItem_5"), nullptr);
        const Variant loops = decodeMetadata(c.readData(file, *c.find("ImageMetadata!")));
        EXPECT_NE(loops.path({ "uLoopPars", "i0000000000", "uiCount" }), nullptr);
    }
}

TEST(Nd2Variant, ToJson)
{
    Bytes ba = le<uint64_t>(3);
    append(ba, Bytes{ 1, 2, 3 });
    Bytes data;
    append(data, lite(9, u"blob", ba));
    append(data, level(u"list", { lite(3, u"", le<uint32_t>(1)), lite(8, u"", utf16(u"two")) }));
    append(data, lite(1, u"b", Bytes{ 1 }));
    const nlohmann::json j = decode(data).toJson();
    EXPECT_EQ(j["blob"], nlohmann::json({ {"bytes", 3} }));
    EXPECT_TRUE(j["list"].is_array());
    EXPECT_EQ(j["list"][0], 1);
    EXPECT_EQ(j["list"][1], "two");
    EXPECT_EQ(j["b"], true);
}
