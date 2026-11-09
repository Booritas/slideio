// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/core/exceptions.hpp"

using namespace slideio;
using namespace slideio::bif;

TEST(BIFDescription, parsesLevelAndMagnification)
{
    auto desc = parseLevelDescription("level=0 mag=40 quality=95");
    ASSERT_TRUE(desc.has_value());
    EXPECT_EQ(0, desc->level);
    EXPECT_DOUBLE_EQ(40., desc->magnification);
    EXPECT_EQ("95", desc->tokens.at("quality"));
}

TEST(BIFDescription, parsesFractionalMagnification)
{
    auto desc = parseLevelDescription("level=6 mag=0.625 quality=90");
    ASSERT_TRUE(desc.has_value());
    EXPECT_EQ(6, desc->level);
    EXPECT_DOUBLE_EQ(0.625, desc->magnification);
}

TEST(BIFDescription, noLevelTokenGivesNullopt)
{
    EXPECT_FALSE(parseLevelDescription("Label Image").has_value());
}

TEST(BIFDescription, levelWithoutMagThrows)
{
    EXPECT_THROW(parseLevelDescription("level=1"), slideio::RuntimeError);
}

TEST(BIFDescription, unparsableValuesThrow)
{
    EXPECT_THROW(parseLevelDescription("level=x mag=40"), slideio::RuntimeError);
    EXPECT_THROW(parseLevelDescription("level=1 mag=abc"), slideio::RuntimeError);
}

TEST(BIFDescription, classifiesAuxDescriptions)
{
    EXPECT_EQ(AuxKind::Label, classifyAuxDescription("Label Image"));
    EXPECT_EQ(AuxKind::Label, classifyAuxDescription("Label_Image"));
    EXPECT_EQ(AuxKind::Thumbnail, classifyAuxDescription("Thumbnail"));
    EXPECT_EQ(AuxKind::Probability, classifyAuxDescription("Probability_Image"));
    EXPECT_EQ(AuxKind::None, classifyAuxDescription("label image"));
    EXPECT_EQ(AuxKind::None, classifyAuxDescription(""));
}

TEST(BIFDescription, auxNames)
{
    EXPECT_EQ("Label", auxImageName(AuxKind::Label));
    EXPECT_EQ("Thumbnail", auxImageName(AuxKind::Thumbnail));
    EXPECT_EQ("Probability", auxImageName(AuxKind::Probability));
}

TEST(BIFDescription, partialNumbersThrow)
{
    EXPECT_THROW(parseLevelDescription("level=1x mag=40"), slideio::RuntimeError);
    EXPECT_THROW(parseLevelDescription("level=1 mag=40,5"), slideio::RuntimeError);
    EXPECT_THROW(parseLevelDescription("level=1.5 mag=40"), slideio::RuntimeError);
}
