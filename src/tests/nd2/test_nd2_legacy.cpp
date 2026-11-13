// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "slideio/drivers/nd2/nd2legacy.hpp"
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2slide.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "tests/nd2/nd2_fixture_values.hpp"

using namespace slideio;
using namespace slideio::nd2;

// legacy.nd2:    8x6, 16 bits, 2 channels, 3 frames (T loop 3 + a spectral loop), the
//                second channel of frame 2 has no codestream; calibration 0.25 um.
// legacy_xy.nd2: 8x6, 8 bits, 1 channel, 4 frames; T 2 x an XY loop of 4 valid points,
//                but the TINF text says XY(2), which rules (FD 11.3).
// Both written by write_legacy in auxfiles/nd2-fixtures/generate.py.

namespace
{
    std::string fixture(const std::string& name)
    {
        return std::string(ND2_TEST_DATA_DIR) + "/" + name;
    }

    cv::Mat readPlane(const std::shared_ptr<CVScene>& scene, int t, int channel)
    {
        cv::Mat out;
        const cv::Rect rect = scene->getRect();
        scene->readResampledBlockChannelsEx(rect, rect.size(), { channel }, 0, t, out);
        return out;
    }

    void expectPlane(const cv::Mat& plane, int k, int64_t frame, DataType type, const std::string& what)
    {
        cv::Mat d;
        plane.convertTo(d, CV_64F);
        for (int y = 0; y < d.rows; ++y) {
            for (int x = 0; x < d.cols; ++x) {
                const double expected = nd2Expected(x, y, k, frame, 0, type);
                if (d.at<double>(y, x) != expected) {
                    ADD_FAILURE() << what << ": pixel (" << x << ", " << y << ") is " << d.at<double>(y, x)
                                  << ", expected " << expected;
                    return;
                }
            }
        }
    }
}

TEST(Nd2Legacy, Boxes)
{
    FileReader file(fixture("legacy.nd2"));
    const LegacyFile legacy = LegacyFile::read(file);
    EXPECT_EQ(5u, legacy.codestreams().size());
    EXPECT_EQ(8, legacy.width());
    EXPECT_EQ(6, legacy.height());
    EXPECT_EQ(1, legacy.components());
    EXPECT_EQ(16, legacy.bits());
    ASSERT_NE(nullptr, legacy.first("VIMD"));
    EXPECT_EQ("xml ", legacy.first("VIMD")->type);
    EXPECT_NE(nullptr, legacy.first("AIM1"));
    EXPECT_NE(nullptr, legacy.first("TINF"));
    EXPECT_EQ(nullptr, legacy.first("ACAL"));
    // PR review P2: a metadata box longer than the bound is refused before it is allocated.
    const Box& vimd = *legacy.first("VIMD");
    EXPECT_ANY_THROW(legacy.readBox(file, vimd, vimd.dataLength - 1));
    EXPECT_EQ(vimd.dataLength, legacy.readBox(file, vimd).size());
    for (const Box& box : legacy.codestreams()) {
        EXPECT_EQ("jp2c", box.type);
        const std::vector<uint8_t> data = legacy.readBox(file, box);
        ASSERT_GE(data.size(), 4u);
        EXPECT_EQ(0xFF, data[0]);   // SOC
        EXPECT_EQ(0x4F, data[1]);
    }
}

TEST(Nd2Legacy, XyText)
{
    EXPECT_EQ(5, xyCountFromText("Metadata:\r\nDimensions: T'(81) x XY(5) x \xCE\xBB(2)"));
    EXPECT_EQ(-1, xyCountFromText("Metadata:\r\nDimensions: T(3) x \xCE\xBB(2)\r\n"));
    EXPECT_EQ(4, xyCountFromText("Metadata:\r\nDimensions: T'(11) x XY(4) x Z(5) x \xCE\xBB(1)\r\nCamera: x"));
    EXPECT_EQ(-1, xyCountFromText("Metadata:\r\nCamera: XY(3)"));   // not on the Dimensions line
    EXPECT_EQ(-1, xyCountFromText(""));
}

TEST(Nd2Legacy, Dimensions)
{
    auto xy = ND2Slide::openFile(fixture("legacy_xy.nd2"));
    ASSERT_EQ(2, xy->getNumScenes());
    for (int p = 0; p < 2; ++p) {
        auto scene = xy->getScene(p);
        EXPECT_EQ(2, scene->getNumTFrames());
        EXPECT_EQ(1, scene->getNumZSlices());
        EXPECT_EQ(1, scene->getNumChannels());
        EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
        EXPECT_EQ(cv::Rect(0, 0, 8, 6), scene->getRect());
    }
    EXPECT_EQ("Position 1", xy->getScene(0)->getName());
    EXPECT_EQ("Position 2", xy->getScene(1)->getName());

    auto slide = ND2Slide::openFile(fixture("legacy.nd2"));
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    EXPECT_EQ("legacy", scene->getName());
    // Frame 2 lacks channel 1's codestream, so it adds no time point (R25), as Bio-Formats
    // gives T 50 for b16_14_12.nd2.
    EXPECT_EQ(2, scene->getNumTFrames());
    EXPECT_EQ(1, scene->getNumZSlices());
    ASSERT_EQ(2, scene->getNumChannels());
    EXPECT_EQ(DataType::DT_UInt16, scene->getChannelDataType(0));
    EXPECT_EQ("CH0", scene->getChannelName(0));
    EXPECT_EQ("CH1", scene->getChannelName(1));
    EXPECT_DOUBLE_EQ(0.25e-6, scene->getResolution().x);
    EXPECT_DOUBLE_EQ(0.25e-6, scene->getResolution().y);
    EXPECT_DOUBLE_EQ(0.1, scene->getTFrameResolution());
    EXPECT_DOUBLE_EQ(20., scene->getMagnification());

    const std::string raw = slide->getRawMetadata();
    for (const char* key : { "\"version\":\"1.0\"", "\"VIMD\"", "\"VCAL\"", "\"AIM1\"", "\"TINF\"" }) {
        EXPECT_NE(std::string::npos, raw.find(key)) << key;
    }
}

TEST(Nd2Legacy, FrameTimes)
{
    auto file = std::make_shared<const FileReader>(fixture("legacy.nd2"));
    const LegacyFile legacy = LegacyFile::read(*file);
    const ImageInfo info = buildImageInfo(legacy, file, "legacy");
    ASSERT_EQ(2, info.frameTimesMs.size());
    EXPECT_DOUBLE_EQ(0., info.frameTimesMs[0]);
    EXPECT_DOUBLE_EQ(100., info.frameTimesMs[1]);
    EXPECT_DOUBLE_EQ(2460000.5, info.absoluteJulianDay);
}

// A frame whose codestreams are not all there is present: it reads its existing channel
// and background for the missing one (R25). In legacy.nd2 it lies beyond T, so the frame
// source is asked directly; inside T (P or Z inner) the scene reads it the same way.
TEST(Nd2Legacy, PartialFrameReadsBackground)
{
    auto file = std::make_shared<const FileReader>(fixture("legacy.nd2"));
    const LegacyFile legacy = LegacyFile::read(*file);
    const auto frames = makeJp2FrameSource(file, legacy, 2);
    ASSERT_TRUE(frames->hasFrame(2));
    EXPECT_FALSE(frames->hasFrame(3));
    cv::Mat both;
    frames->readRows(2, cv::Rect(0, 0, 8, 6), both);
    ASSERT_EQ(2, both.channels());
    cv::Mat planes[2];
    cv::split(both, planes);
    expectPlane(planes[0], 0, 2, DataType::DT_UInt16, "legacy frame 2 channel 0");
    EXPECT_EQ(0, cv::countNonZero(planes[1]));
}

// PR review: a JPEG 2000 cursor decodes the frame once and serves every tile from it.
TEST(Nd2Legacy, CursorMatchesReadRows)
{
    auto file = std::make_shared<const FileReader>(fixture("legacy.nd2"));
    const LegacyFile legacy = LegacyFile::read(*file);
    const auto frames = makeJp2FrameSource(file, legacy, 2);
    for (int64_t frame : { 0, 2 }) {   // frame 2 lacks its second channel's codestream
        SCOPED_TRACE(frame);
        auto cursor = frames->openCursor(frame);
        for (const cv::Rect& rect : { cv::Rect(0, 0, 4, 3), cv::Rect(4, 0, 4, 3), cv::Rect(0, 3, 8, 3),
                                      cv::Rect(0, 0, 8, 6), cv::Rect(2, 1, 5, 4) }) {
            cv::Mat expected, actual;
            frames->readRows(frame, rect, expected);
            cursor->readRows(rect, actual);
            ASSERT_EQ(expected.size(), actual.size());
            ASSERT_EQ(expected.type(), actual.type());
            EXPECT_EQ(0, cv::norm(expected, actual, cv::NORM_INF)) << rect;
        }
    }
}

TEST(Nd2Legacy, Pixels)
{
    auto xy = ND2Slide::openFile(fixture("legacy_xy.nd2"));
    for (int p = 0; p < xy->getNumScenes(); ++p) {
        auto scene = xy->getScene(p);
        for (int t = 0; t < scene->getNumTFrames(); ++t) {
            const int64_t frame = t * 2 + p;   // T outer, XY inner
            expectPlane(readPlane(scene, t, 0), 0, frame, DataType::DT_Byte,
                "legacy_xy p=" + std::to_string(p) + " t=" + std::to_string(t));
        }
    }

    auto scene = ND2Slide::openFile(fixture("legacy.nd2"))->getScene(0);
    for (int t = 0; t < scene->getNumTFrames(); ++t) {
        for (int k = 0; k < scene->getNumChannels(); ++k) {
            const cv::Mat plane = readPlane(scene, t, k);
            const std::string what = "legacy t=" + std::to_string(t) + " k=" + std::to_string(k);
            expectPlane(plane, k, t, DataType::DT_UInt16, what);
        }
    }

    // Both channels at once, from a sub-rectangle.
    cv::Mat both;
    const cv::Rect rect(2, 1, 5, 4);
    scene->readResampledBlockChannelsEx(rect, rect.size(), {}, 0, 1, both);
    ASSERT_EQ(2, both.channels());
    ASSERT_EQ(rect.size(), both.size());
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const cv::Vec<uint16_t, 2> v = both.at<cv::Vec<uint16_t, 2>>(y, x);
            EXPECT_EQ(nd2Expected(rect.x + x, rect.y + y, 0, 1, 0, DataType::DT_UInt16), v[0]);
            EXPECT_EQ(nd2Expected(rect.x + x, rect.y + y, 1, 1, 0, DataType::DT_UInt16), v[1]);
        }
    }
}

TEST(Nd2Legacy, Compression)
{
    EXPECT_EQ(Compression::Jpeg2000, ND2Slide::openFile(fixture("legacy.nd2"))->getScene(0)->getCompression());
    EXPECT_EQ(Compression::Jpeg2000, ND2Slide::openFile(fixture("legacy_xy.nd2"))->getScene(0)->getCompression());
}

TEST(Nd2Legacy, BoxMapMustMatchWalk)
{
    // Move the first map entry's offset (the jP signature box at 0) by one byte.
    std::ifstream in(fixture("legacy.nd2"), std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_GT(bytes.size(), 40u);
    uint64_t distance = 0;
    std::memcpy(&distance, bytes.data() + bytes.size() - 8, sizeof(distance));   // little-endian host
    const size_t entry = bytes.size() - static_cast<size_t>(distance) + 4;      // after the u32 count
    bytes[entry + 8] = 1;
    TempFile copy("nd2");
    {
        std::ofstream out(copy.getPath(), std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    const std::string path = copy.getPath().string();
    try {
        ND2Slide::openFile(path);
        ADD_FAILURE() << "a box map entry that is not a box start must fail the open";
    }
    catch (const std::exception& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("ND2: legacy box map")) << e.what();
    }
}

TEST(Nd2Legacy, FromLoopsValidates)
{
    Loop t;
    t.axis = Axis::T;
    t.count = 2;
    Loop p;
    p.axis = Axis::P;
    p.count = 3;
    p.validItems = { 0, 1, 2 };
    const Experiment e = Experiment::fromLoops({ t, p });
    EXPECT_EQ(6, e.loopProduct());
    EXPECT_EQ(5, e.frameIndex(1, 2, 0));
    EXPECT_THROW(Experiment::fromLoops({ t, t }), Unsupported);
    Loop huge;
    huge.axis = Axis::Z;
    huge.count = int64_t(1) << 62;
    EXPECT_THROW(Experiment::fromLoops({ t, p, huge }), Unsupported);
    EXPECT_EQ(1, Experiment::fromLoops({}).loopProduct());
}

TEST(Nd2Legacy, RealShapedXml)
{
    // A legacy VIMD as NIS writes it: a LIM comment, a <MetadataSeq> root around the
    // <variant>, prefixed names; and a TINF whose text sits in attributes.
    const std::string vimd =
        "<!--Laboratory Imaging(http://www.lim.cz) generated xml-->\r\n"
        "<MetadataSeq _SEQUENCE_INDEX=\"0\"><variant version=\"1.0\"><no_name runtype=\"RLxPictureMetadata\">"
        "<dTimeMSec runtype=\"double\" value=\"5830.5\"/>"
        "<sPicturePlanes runtype=\"RLxPicturePlanes\"><uiCount runtype=\"lx_uint32\" value=\"2\"/>"
        "<sPlane runtype=\"CLxListVariant\"><a0 runtype=\"x\"><sDescription runtype=\"CLxStringW\" value=\"rfp\"/></a0>"
        "<a1 runtype=\"x\"><sDescription runtype=\"CLxStringW\" value=\"phase\"/></a1></sPlane></sPicturePlanes>"
        "</no_name></variant></MetadataSeq>";
    const Variant v = decodeLegacyXml(std::vector<uint8_t>(vimd.begin(), vimd.end()));
    ASSERT_NE(nullptr, v.get("dTimeMSec"));
    EXPECT_DOUBLE_EQ(5830.5, v.get("dTimeMSec")->asDouble());
    const std::vector<ChannelInfo> channels = parseChannels(&v, 2);
    ASSERT_EQ(2u, channels.size());
    EXPECT_EQ("rfp", channels[0].name);
    EXPECT_EQ("phase", channels[1].name);

    const std::string tinf =
        "<!--Laboratory Imaging(http://www.lim.cz) generated xml-->\r\n"
        "<TextInfo Count=\"2\" _DOCTYPE=\"TextInfoDocument\" _VERSION=\"1.000000\">"
        "<TextInfoItem Text=\"\" Index=\"0\"/>"
        "<TextInfoItem Text=\"Metadata:&#x000d;&#x000a;Dimensions: T'(81) x XY(5)&#x000d;&#x000a;\" Index=\"1\"/>"
        "</TextInfo>";
    const Variant t = decodeLegacyXml(std::vector<uint8_t>(tinf.begin(), tinf.end()));
    const Variant* items = t.get("TextInfoItem");
    ASSERT_NE(nullptr, items);
    ASSERT_EQ(2u, items->items().size());
    const Variant* text = items->items()[1].get("Text");
    ASSERT_NE(nullptr, text);
    EXPECT_EQ(5, xyCountFromText(text->asString()));
}

// PR review P1: an indexed Map is reachable both as members and as items, so a walk
// over both visited 2^depth nodes. 60 nested i0000000000 levels must stay instant.
TEST(Nd2Legacy, NestedIndexedTinfIsLinear)
{
    std::string xml = "<TextInfo Count=\"1\">";
    const int depth = 60;
    for (int i = 0; i < depth; ++i) {
        xml += "<i0000000000 runtype=\"x\">";
    }
    xml += "<Index runtype=\"lx_int32\" value=\"1\"/>";
    for (int i = 0; i < depth; ++i) {
        xml += "</i0000000000>";
    }
    xml += "<TextInfoItem Text=\"Metadata:&#x000d;&#x000a;Dimensions: XY(3)\" Index=\"0\"/></TextInfo>";
    const Variant tinf = decodeLegacyXml(std::vector<uint8_t>(xml.begin(), xml.end()));
    const auto start = std::chrono::steady_clock::now();
    const std::string text = tinfMetadataText(tinf);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(3, xyCountFromText(text));
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}

// PR review: legacy_12bit.nd2 is 8x6, 1 channel, T2, with 12-bit codestreams (ihdr BPC 11).
// It reads as 16-bit samples whose significant bits are 12.
TEST(Nd2Legacy, TwelveBitCodestreams)
{
    FileReader file(fixture("legacy_12bit.nd2"));
    EXPECT_EQ(12, LegacyFile::read(file).bits());
    auto slide = ND2Slide::openFile(fixture("legacy_12bit.nd2"));
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    ASSERT_EQ(1, scene->getNumChannels());
    ASSERT_EQ(2, scene->getNumTFrames());
    EXPECT_EQ(DataType::DT_UInt16, scene->getChannelDataType(0));
    EXPECT_EQ(12, scene->getChannelSignificantBits(0));
    for (int t = 0; t < 2; ++t) {
        expectPlane(readPlane(scene, t, 0), 0, t, DataType::DT_UInt16, "legacy_12bit t=" + std::to_string(t));
    }
}

TEST(Nd2Legacy, RgbWithoutPicturePlanes)
{
    // legacy_rgb.nd2: 8x6, 8 bits, ihdr 3 components, 2 frames (one RGB codestream each),
    // and a VIMD with no PicturePlanes at all: one ND2 channel of 3 components.
    auto slide = ND2Slide::openFile(fixture("legacy_rgb.nd2"));
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    ASSERT_EQ(3, scene->getNumChannels());
    EXPECT_EQ(2, scene->getNumTFrames());
    EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
    EXPECT_EQ("R", scene->getChannelName(0));
    EXPECT_EQ("G", scene->getChannelName(1));
    EXPECT_EQ("B", scene->getChannelName(2));
    const Metadata& attrs = scene->getChannelAttributes();
    for (int k = 0; k < 3; ++k) {
        EXPECT_EQ(0, attrs[size_t(k)]["ND2Channel"].asInt()) << k;
        EXPECT_EQ(k, attrs[size_t(k)]["Component"].asInt()) << k;
    }
    for (int t = 0; t < 2; ++t) {
        for (int k = 0; k < 3; ++k) {
            expectPlane(readPlane(scene, t, k), k, t, DataType::DT_Byte,
                "legacy_rgb t=" + std::to_string(t) + " k=" + std::to_string(k));
        }
    }

    auto file = std::make_shared<const FileReader>(fixture("legacy_rgb.nd2"));
    const LegacyFile legacy = LegacyFile::read(*file);
    EXPECT_EQ(3, legacy.components());
    EXPECT_EQ(2u, legacy.codestreams().size());
    const ImageInfo info = buildImageInfo(legacy, file, "legacy_rgb");
    ASSERT_EQ(3u, info.channels.size());
    for (const ChannelInfo& ch : info.channels) {
        EXPECT_EQ(0, ch.nd2Channel);   // every component is in the one codestream per frame
    }
}

// Design 4.5 (final review): the TINF XY(n) count is bounded like the loops, so XY(4000000000)
// is refused before 4e9 points are listed.
TEST(Nd2Legacy, HugeTextXyIsRefused)
{
    std::ifstream in(fixture("legacy_xy.nd2"), std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string from = "T(2) x XY(2)&#13;&#10;Camera: fixture";
    const std::string to = "XY(4000000000)&#13;&#10;Camera: fixtu";   // same length: no box moves
    ASSERT_EQ(from.size(), to.size());
    const size_t at = bytes.find(from);
    ASSERT_NE(std::string::npos, at);
    bytes.replace(at, from.size(), to);
    TempFile copy("nd2");
    {
        std::ofstream out(copy.getPath(), std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    try {
        ND2Slide::openFile(copy.getPath().string());
        ADD_FAILURE() << "a text XY count the file cannot hold must fail the open";
    }
    catch (const std::exception& e) {   // the slide appends the file name
        EXPECT_NE(std::string::npos, std::string(e.what()).find(
            "the text info says XY(4000000000), more than the 5120 frames the file can justify")) << e.what();
    }
}
