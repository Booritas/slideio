// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/tools.hpp"


TEST(Tools, matchPattern)
{
#if defined(WIN32)    
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.tiff;*.czi"));
    EXPECT_FALSE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.tiff;*.aczi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.OME.TIFF","*.atiff;*.aczi;*.ome.tiff"));
#else
    EXPECT_TRUE(slideio::Tools::matchPattern("ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("/root/abs.ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("/root/abs.ad.czi","*.tiff;*.czi"));
    EXPECT_FALSE(slideio::Tools::matchPattern("/root/abs.ad.czi","*.tiff;*.aczi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("/root/abs.ad.ome.tiff","*.atiff;*.aczi;*.ome.tiff"));
#endif
}
TEST(Tools, findZoomLevel)
{
    struct ZoomLevel
    {
        double zoom;
        double getZoom() const { return zoom; }
    };
    std::vector<ZoomLevel> levels;
    levels.resize(10);
    double zoom = 1.;
    for (auto& level : levels)
    {
        level.zoom = zoom;
        zoom /= 2.;
    }
    double lastZoom = levels.back().zoom;
    auto zoomFunct = [&levels](int level)
    {
        return levels[level].zoom;
    };
    const int numLevels = static_cast<int>(levels.size());
    EXPECT_EQ(slideio::Tools::findZoomLevel(2., numLevels, zoomFunct), 0);
    EXPECT_EQ(slideio::Tools::findZoomLevel(lastZoom, numLevels, zoomFunct), 9);
    EXPECT_EQ(slideio::Tools::findZoomLevel(lastZoom * 2, numLevels, zoomFunct), 8);
    EXPECT_EQ(slideio::Tools::findZoomLevel(lastZoom / 2, numLevels, zoomFunct), 9);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.5, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.501, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.499, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.25, numLevels, zoomFunct), 2);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.125, numLevels, zoomFunct), 3);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.55, numLevels, zoomFunct), 0);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.45, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.1, numLevels, zoomFunct), 3);
}

TEST(Tools, convert12BitsTo16Bits2vals)
{
    int16_t src1 = 0xE0F;
    int16_t src2 = 0xF0E;
    int32_t srcB = src1 | (src2 << 12);
    uint16_t trg[2] = { 0 };
    slideio::Tools::convert12BitsTo16Bits((uint8_t*)&srcB, trg, 2);
    EXPECT_EQ(src1, trg[0]);
    EXPECT_EQ(src2, trg[1]);
}

TEST(Tools, convert12BitsTo16Bits3vals)
{
    int16_t src1 = 0xE0F;
    int16_t src2 = 0xF0E;
    int16_t src3 = 0xA0B;
    uint8_t src[8];
    int32_t src32= src1 | (src2 << 12);
    memcpy(src, &src32, sizeof(src32));
    src32 = src3;
    memcpy(src+3, &src32, sizeof(src32));
    uint16_t trg[3] = { 0 };
    slideio::Tools::convert12BitsTo16Bits(src, trg, 3);
    EXPECT_EQ(src1, trg[0]);
    EXPECT_EQ(src2, trg[1]);
    EXPECT_EQ(src3, trg[2]);
}