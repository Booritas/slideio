// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/tools.hpp"


TEST(Tools, matchPattern)
{
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.tiff;*.czi"));
    EXPECT_FALSE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.tiff;*.aczi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.OME.TIFF","*.tiff;*.aczi;*.ome.tiff"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs\\SLIDEIO123.OME.TIFF","*.tiff;*.aczi;slideio*.ome.tiff"));
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
