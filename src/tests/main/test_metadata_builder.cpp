// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/metadata.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/base/exceptions.hpp"
#include <nlohmann/json.hpp>

using namespace slideio;

TEST(MetadataBuilder, DefaultIsNull)
{
    MetadataBuilder b;
    EXPECT_TRUE(b.isNull());

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Null);
}

TEST(MetadataBuilder, SetStringRoundtrip)
{
    MetadataBuilder b;
    b.set(std::string("hello"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::String);
    EXPECT_EQ(m.asString(), "hello");
}

TEST(MetadataBuilder, SetTypedLeafRoundtrip)
{
    {
        MetadataBuilder b;
        b.set(true);
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Bool);
        EXPECT_TRUE(m.asBool());
    }
    {
        MetadataBuilder b;
        b.set(static_cast<int64_t>(488));
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Int);
        EXPECT_EQ(m.asInt(), 488);
    }
    {
        MetadataBuilder b;
        b.set(0.1);
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Double);
        EXPECT_DOUBLE_EQ(m.asDouble(), 0.1);
    }
    {
        MetadataBuilder b;
        b.set("literal");
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::String);
        EXPECT_EQ(m.asString(), "literal");
    }
}

TEST(MetadataBuilder, SetConstCharNullThrows)
{
    MetadataBuilder b;
    EXPECT_THROW(b.set(static_cast<const char*>(nullptr)), slideio::RuntimeError);
}

TEST(MetadataBuilder, ObjectKeyAutoCreate)
{
    MetadataBuilder b;
    b["wavelength"].set(std::string("488nm"));
    b["exposure"].set(std::string("100ms"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_TRUE(m.contains("wavelength"));
    EXPECT_TRUE(m.contains("exposure"));
    EXPECT_EQ(m["wavelength"].asString(), "488nm");
    EXPECT_EQ(m["exposure"].asString(), "100ms");
}

TEST(MetadataBuilder, MakeObjectIdempotent)
{
    MetadataBuilder b;
    b.makeObject();
    EXPECT_TRUE(b.isObject());

    b["a"].set(std::string("1"));
    b.makeObject(); // idempotent — must not clobber "a"
    EXPECT_TRUE(b.isObject());

    Metadata m = b.freeze();
    EXPECT_TRUE(m.contains("a"));
    EXPECT_EQ(m["a"].asString(), "1");
}

TEST(MetadataBuilder, MakeObjectOnScalarReplaces)
{
    MetadataBuilder b;
    b.set(std::string("scalar"));
    b.makeObject(); // replaces the scalar

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_EQ(m.size(), 0u);
}

TEST(MetadataBuilder, ArrayIndexAutoGrow)
{
    MetadataBuilder b;
    b[2]["wavelength"].set(std::string("640nm"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Array);
    EXPECT_EQ(m.size(), 3u);                        // grown to index+1
    EXPECT_EQ(m[0].type(), Metadata::Type::Object); // new slots default to empty Object
    EXPECT_EQ(m[0].size(), 0u);
    EXPECT_EQ(m[1].type(), Metadata::Type::Object);
    EXPECT_EQ(m[1].size(), 0u);
    EXPECT_EQ(m[2]["wavelength"].asString(), "640nm");
}

TEST(MetadataBuilder, MakeArrayIdempotent)
{
    MetadataBuilder b;
    b.makeArray();
    EXPECT_TRUE(b.isArray());

    b[0].set(std::string("first"));
    b.makeArray(); // idempotent — must not clobber [0]
    EXPECT_TRUE(b.isArray());

    Metadata m = b.freeze();
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].asString(), "first");
}

TEST(MetadataBuilder, SizeReflectsContainer)
{
    MetadataBuilder b;
    EXPECT_EQ(b.size(), 0u); // Null reports 0

    b.makeObject();
    EXPECT_EQ(b.size(), 0u);
    b["k"].set(std::string("v"));
    EXPECT_EQ(b.size(), 1u);
}

TEST(MetadataBuilder, OperatorKeyOnArrayThrows)
{
    MetadataBuilder b;
    b.makeArray();
    EXPECT_THROW(b["k"], slideio::RuntimeError);
}

TEST(MetadataBuilder, OperatorKeyOnScalarThrows)
{
    MetadataBuilder b;
    b.set(static_cast<int64_t>(42));
    EXPECT_THROW(b["k"], slideio::RuntimeError);
}

TEST(MetadataBuilder, OperatorIndexOnObjectThrows)
{
    MetadataBuilder b;
    b.makeObject();
    EXPECT_THROW(b[size_t{0}], slideio::RuntimeError);
}

TEST(MetadataBuilder, OperatorIndexOnScalarThrows)
{
    MetadataBuilder b;
    b.set(true);
    EXPECT_THROW(b[size_t{0}], slideio::RuntimeError);
}

TEST(MetadataBuilder, TopLevelCopyIsIndependent)
{
    MetadataBuilder a;
    a["k"].set(std::string("a-value"));

    MetadataBuilder b = a; // top-level copy
    b["k"].set(std::string("b-value"));

    EXPECT_EQ(a.freeze()["k"].asString(), "a-value"); // a unchanged
    EXPECT_EQ(b.freeze()["k"].asString(), "b-value");
}

TEST(MetadataBuilder, SubViewSharesStorageWithParent)
{
    MetadataBuilder parent;
    MetadataBuilder child = parent["channels"];
    child[0]["wavelength"].set(std::string("488nm"));

    Metadata m = parent.freeze(); // parent sees the child's writes
    ASSERT_TRUE(m.contains("channels"));
    EXPECT_EQ(m["channels"][0]["wavelength"].asString(), "488nm");
}

TEST(MetadataBuilder, FreezeIsIndependentOfLaterMutation)
{
    MetadataBuilder b;
    b["k"].set(std::string("before"));

    Metadata snapshot = b.freeze();
    b["k"].set(std::string("after")); // mutate after freeze

    EXPECT_EQ(snapshot["k"].asString(), "before"); // snapshot unchanged
    EXPECT_EQ(b.freeze()["k"].asString(), "after");
}

TEST(MetadataBuilder, BuilderFromJsonRoundtrip)
{
    nlohmann::json j = {{"channels", nlohmann::json::array({{{"wavelength", "488nm"}, {"exposure", "100ms"}},
                                                            {{"wavelength", "561nm"}}})}};
    MetadataBuilder b = slideio::detail::builderFromJson(j);

    Metadata m = b.freeze();
    EXPECT_TRUE(m.contains("channels"));
    EXPECT_EQ(m["channels"].size(), 2u);
    EXPECT_EQ(m["channels"][0]["wavelength"].asString(), "488nm");
    EXPECT_EQ(m["channels"][0]["exposure"].asString(), "100ms");
    EXPECT_EQ(m["channels"][1]["wavelength"].asString(), "561nm");
}

TEST(MetadataBuilder, MakeDefaultMetadataBuilderTextFormat)
{
    MetadataBuilder b = slideio::detail::makeDefaultMetadataBuilder("raw text content", slideio::MetadataFormat::Text);
    Metadata m = b.freeze();
    EXPECT_TRUE(m.contains("text"));
    EXPECT_EQ(m["text"].asString(), "raw text content");
}

TEST(MetadataBuilder, MakeDefaultMetadataBuilderJsonFormat)
{
    MetadataBuilder b =
        slideio::detail::makeDefaultMetadataBuilder(R"({"a": 1, "b": "two"})", slideio::MetadataFormat::JSON);
    Metadata m = b.freeze();
    EXPECT_EQ(m["a"].asInt(), 1);
    EXPECT_EQ(m["b"].asString(), "two");
}

TEST(MetadataBuilder, MakeDefaultMetadataBuilderNoneFormat)
{
    MetadataBuilder b = slideio::detail::makeDefaultMetadataBuilder("", slideio::MetadataFormat::None);
    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_EQ(m.size(), 0u);
}
