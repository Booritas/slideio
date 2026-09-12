#include <gtest/gtest.h>
#include "slideio/core/colorprofile.hpp"

using namespace slideio;

TEST(ColorProfile, defaultIsAbsent)
{
    ColorProfile profile;
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(0u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
    ASSERT_TRUE(profile.getData().empty());
}

TEST(ColorProfile, constructedFromBytesIsEmbedded)
{
    std::vector<uint8_t> bytes{1, 2, 3, 4};
    ColorProfile profile(bytes);
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(4u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::Embedded, profile.getSource());
    ASSERT_EQ(bytes, profile.getData());
}

TEST(ColorProfile, emptyByteVectorStaysAbsent)
{
    // A driver that finds a zero-length tag must not claim a profile exists.
    ColorProfile profile(std::vector<uint8_t>{});
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
}

TEST(ColorProfile, sourceIsSettable)
{
    ColorProfile profile(std::vector<uint8_t>{1, 2, 3});
    profile.setSource(ColorProfileSource::Assumed);
    ASSERT_EQ(ColorProfileSource::Assumed, profile.getSource());
}

TEST(ColorProfileInfo, defaultIsNotPresent)
{
    ColorProfileInfo info;
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
    ASSERT_EQ(IccColorSpace::Unknown, info.dataSpace);
    ASSERT_EQ(0u, info.dataSize);
}

TEST(ColorProfileInfo, toStringNamesPresenceAndDescription)
{
    ColorProfileInfo info;
    info.present = true;
    info.source = ColorProfileSource::Embedded;
    info.description = "Aperio RGB";
    info.dataSpace = IccColorSpace::RGB;
    info.dataSize = 3144;
    const std::string text = info.toString();
    ASSERT_NE(std::string::npos, text.find("Aperio RGB"));
    ASSERT_NE(std::string::npos, text.find("Embedded"));
    ASSERT_NE(std::string::npos, text.find("3144"));
}
