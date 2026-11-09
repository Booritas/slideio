// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/exceptions.hpp"
#include "bif_xml_fixtures.hpp"

using namespace slideio;
using namespace slideio::bif;

namespace
{
    std::string replaceOnce(std::string text, const std::string& from, const std::string& to)
    {
        const size_t pos = text.find(from);
        EXPECT_NE(std::string::npos, pos) << from;
        if (pos != std::string::npos) {
            text.replace(pos, from.size(), to);
        }
        return text;
    }

    const char* usableDown =
        "<TileJointInfo FlagJoined=\"1\" Confidence=\"100\" Direction=\"DOWN\" Tile1=\"1\" Tile2=\"2\"/>\n";
}

TEST(BIFXmlAcquisition, os2)
{
    const AcquisitionInfo info = parseAcquisitionXml(fixtures::os2Acquisition);
    EXPECT_DOUBLE_EQ(0.2325, info.scanResMicrons);
    EXPECT_DOUBLE_EQ(40., info.magnification);
    EXPECT_EQ(1, info.zLayers);
}

TEST(BIFXmlAcquisition, ventana)
{
    const AcquisitionInfo info = parseAcquisitionXml(fixtures::ventanaAcquisition);
    EXPECT_EQ("VENTANA DP 200", info.scannerModel);
    EXPECT_DOUBLE_EQ(0.25, info.scanResMicrons);
}

TEST(BIFXmlAcquisition, notBif)
{
    try {
        parseAcquisitionXml("<foo/>");
        FAIL() << "expected an exception";
    }
    catch (const std::exception& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("no iScan metadata"));
    }
}

TEST(BIFXmlSnake, threeByThree)
{
    EXPECT_EQ(cv::Point(0, 2), snakePosition(1, 3, 3));
    EXPECT_EQ(cv::Point(2, 2), snakePosition(3, 3, 3));
    EXPECT_EQ(cv::Point(2, 1), snakePosition(4, 3, 3));
    EXPECT_EQ(cv::Point(0, 1), snakePosition(6, 3, 3));
    EXPECT_EQ(cv::Point(0, 0), snakePosition(7, 3, 3));
    EXPECT_EQ(cv::Point(2, 0), snakePosition(9, 3, 3));
    EXPECT_THROW(snakePosition(0, 3, 3), slideio::RuntimeError);
    EXPECT_THROW(snakePosition(10, 3, 3), slideio::RuntimeError);
}

TEST(BIFXmlStitch, os2)
{
    const StitchInfo info = parseStitchXml(fixtures::os2Stitch());
    ASSERT_EQ(3u, info.aois.size());
    EXPECT_EQ(0, info.aois[0].index);
    EXPECT_EQ(1, info.aois[1].index);
    EXPECT_EQ(2, info.aois[2].index);
    EXPECT_FALSE(info.aois[1].scanned);
    EXPECT_EQ(2u, info.aois[0].joints.size());
    EXPECT_EQ(0, info.aois[0].originX);
    EXPECT_EQ(38080, info.aois[0].originY);
    EXPECT_EQ(1024, info.aois[0].tileWidth);
    EXPECT_EQ(1360, info.aois[0].tileHeight);
    EXPECT_EQ(11, info.aois[0].cols);
    EXPECT_EQ(17, info.aois[0].rows);
    EXPECT_EQ(30720, info.aois[2].originX);
    EXPECT_TRUE(info.aois[1].joints.empty());
    EXPECT_NE(std::string::npos, info.aois[0].imageInfoXml.find("NumCols=\"11\""));
    EXPECT_EQ(std::string::npos, info.aois[0].imageInfoXml.find("TileJointInfo"));
}

TEST(BIFXmlStitch, ventana)
{
    const StitchInfo info = parseStitchXml(fixtures::ventanaStitch());
    ASSERT_EQ(1u, info.aois.size());
    ASSERT_EQ(4u, info.aois[0].joints.size());
    for (const Joint& joint : info.aois[0].joints) {
        EXPECT_TRUE(joint.usable());
    }
    EXPECT_EQ(JointAxis::Horizontal, info.aois[0].joints[0].axis);
    EXPECT_EQ(JointAxis::Vertical, info.aois[0].joints[3].axis);
    EXPECT_DOUBLE_EQ(24., info.aois[0].joints[2].overlapX);
}

TEST(BIFXmlStitch, aoiIndexDisagreesWithName)
{
    const std::string xml = replaceOnce(fixtures::ventanaStitch(), "AOIIndex=\"0\"", "AOIIndex=\"5\"");
    EXPECT_THROW(parseStitchXml(xml), slideio::RuntimeError);
}

TEST(BIFXmlStitch, unsupportedDirectionOnUsableJoint)
{
    try {
        parseStitchXml(fixtures::ventanaStitch(usableDown));
        FAIL() << "expected an exception";
    }
    catch (const std::exception& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("DOWN"));
    }
}

TEST(BIFXmlStitch, unusableJointWithUnknownDirectionIsIgnored)
{
    const std::string joint = replaceOnce(usableDown, "FlagJoined=\"1\"", "FlagJoined=\"0\"");
    const StitchInfo info = parseStitchXml(fixtures::ventanaStitch(joint));
    EXPECT_EQ(4u, info.aois[0].joints.size());
}

TEST(BIFXmlStitch, notNeighbours)
{
    const std::string joint =
        "<TileJointInfo FlagJoined=\"1\" Confidence=\"100\" Direction=\"LEFT\" Tile1=\"1\" Tile2=\"3\"/>\n";
    EXPECT_THROW(parseStitchXml(fixtures::ventanaStitch(joint)), slideio::RuntimeError);
}

TEST(BIFXmlStitch, missingOrigin)
{
    const std::string xml = replaceOnce(fixtures::ventanaStitch(),
        "<AOI0 OriginX=\"0\" OriginY=\"0\"/>", "");
    EXPECT_THROW(parseStitchXml(xml), slideio::RuntimeError);
}

TEST(BIFXmlStitch, truncated)
{
    const std::string xml = fixtures::ventanaStitch();
    EXPECT_THROW(parseStitchXml(xml.substr(0, xml.size() / 2)), slideio::RuntimeError);
}
