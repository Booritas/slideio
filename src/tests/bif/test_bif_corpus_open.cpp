// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The open path: scenes, aux images and metadata of the corpus slides (BIFCorpusOpen,
// corpus only), and the refusal of a .bif file that is not BIF (BIFOpen, corpus-free).
#include <gtest/gtest.h>
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/drivers/bif/bifgeometry.hpp"
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/imagetools/encodeparameters.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/slideio/slideio.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;
using namespace slideio::bif;

namespace
{
    // Scene sizes recomputed from the level-0 stitching XML with the parser and
    // geometry functions, independently of BIFSlide: name -> size, scanned AOIs only.
    std::vector<std::pair<std::string, cv::Size>> expectedScenes(const std::string& path)
    {
        TIFFKeeper tiff;
        tiff.openTiffFile(path);
        std::vector<TiffDirectory> dirs;
        TiffTools::scanFile(tiff.getHandle(), dirs);
        std::string packet;
        for (const auto& dir : dirs) {
            const auto level = parseLevelDescription(dir.description);
            if (level && level->level == 0) {
                packet = TiffTools::readXmlPacket(tiff.getHandle(), dir);
                break;
            }
        }
        const StitchInfo stitch = parseStitchXml(packet);
        const auto medianX = slideMedianOverlap(stitch.aois, JointAxis::Horizontal);
        const auto medianY = slideMedianOverlap(stitch.aois, JointAxis::Vertical);
        std::vector<std::pair<std::string, cv::Size>> scenes;
        for (const auto& aoi : stitch.aois) {
            if (aoi.scanned) {
                const AoiGeometry geometry(aoi, estimateOverlaps(aoi, medianX, medianY));
                scenes.emplace_back("AOI" + std::to_string(aoi.index), geometry.size());
            }
        }
        return scenes;
    }

    void checkScenes(const std::shared_ptr<Slide>& slide, const std::string& path,
                     const std::vector<std::string>& names)
    {
        const auto expected = expectedScenes(path);
        ASSERT_EQ(names.size(), expected.size());
        ASSERT_EQ(static_cast<int>(names.size()), slide->getNumScenes());
        for (int index = 0; index < slide->getNumScenes(); ++index) {
            auto scene = slide->getScene(index);
            EXPECT_EQ(names[index], scene->getName());
            EXPECT_EQ(expected[index].first, scene->getName());
            const auto rect = scene->getRect();
            EXPECT_EQ(0, std::get<0>(rect));
            EXPECT_EQ(0, std::get<1>(rect));
            EXPECT_EQ(expected[index].second.width, std::get<2>(rect)) << scene->getName();
            EXPECT_EQ(expected[index].second.height, std::get<3>(rect)) << scene->getName();
        }
    }

    void checkCommon(const std::shared_ptr<Slide>& slide, double resolution, int levels, bool icc)
    {
        EXPECT_EQ("BIF", slide->getDriverId());
        EXPECT_NE(std::string::npos, slide->getRawMetadata().find("iScan"));
        EXPECT_EQ(MetadataFormat::XML, slide->getMetadataFormat());
        for (int index = 0; index < slide->getNumScenes(); ++index) {
            auto scene = slide->getScene(index);
            const auto res = scene->getResolution();
            EXPECT_DOUBLE_EQ(resolution, std::get<0>(res));
            EXPECT_DOUBLE_EQ(resolution, std::get<1>(res));
            EXPECT_DOUBLE_EQ(40., scene->getMagnification());
            EXPECT_EQ(3, scene->getNumChannels());
            EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
            EXPECT_EQ(levels, scene->getNumZoomLevels());
            EXPECT_EQ(icc, !scene->getColorProfile().isEmpty());
            const std::string metadata = scene->getRawMetadata();
            EXPECT_NE(std::string::npos, metadata.find("ImageInfo"));
            EXPECT_EQ(std::string::npos, metadata.find("TileJointInfo"));
            const std::tuple<int, int, int, int> rect = scene->getRect();
            const LevelInfo* level0 = scene->getLevelInfo(0);
            ASSERT_NE(nullptr, level0);
            EXPECT_EQ(std::get<2>(rect), level0->getSize().width);
            EXPECT_EQ(std::get<3>(rect), level0->getSize().height);
            EXPECT_DOUBLE_EQ(1., level0->getScale());
            for (int level = 1; level < scene->getNumZoomLevels(); ++level) {
                const LevelInfo* info = scene->getLevelInfo(level);
                const int d = 1 << level;
                EXPECT_DOUBLE_EQ(1. / d, info->getScale()) << level;
                EXPECT_EQ(std::get<2>(rect) / d, info->getSize().width) << level;
                EXPECT_EQ(std::get<3>(rect) / d, info->getSize().height) << level;
                EXPECT_DOUBLE_EQ(40. / d, info->getMagnification()) << level;
                EXPECT_EQ(level0->getTileSize().width, info->getTileSize().width) << level;
                EXPECT_EQ(level0->getTileSize().height, info->getTileSize().height) << level;
            }
        }
    }

    std::list<std::string> sorted(std::list<std::string> names)
    {
        names.sort();
        return names;
    }
}

TEST(BIFCorpusOpen, OS1)
{
    const std::string path = TestTools::getTestImagePath("bif/openslide", "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto slide = openSlide(path, "AUTO");
    checkScenes(slide, path, {"AOI0"});
    checkCommon(slide, 0.2325e-6, 10, false);
    EXPECT_EQ((std::list<std::string>{"Label", "Thumbnail"}), sorted(slide->getAuxImageNames()));
}

TEST(BIFCorpusOpen, OS2)
{
    const std::string path = TestTools::getTestImagePath("bif/openslide", "OS-2.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto slide = openSlide(path, "AUTO");
    checkScenes(slide, path, {"AOI0", "AOI2"});
    checkCommon(slide, 0.2325e-6, 10, false);
}

TEST(BIFCorpusOpen, Ventana1)
{
    const std::string path = TestTools::getTestImagePath("bif/openslide", "Ventana-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto slide = openSlide(path, "AUTO");
    checkScenes(slide, path, {"AOI0"});
    checkCommon(slide, 0.25e-6, 8, true);
    const auto rect = slide->getScene(0)->getRect();
    EXPECT_EQ(23432, std::get<2>(rect));
    EXPECT_EQ(21504, std::get<3>(rect));
    EXPECT_EQ((std::list<std::string>{"Label", "Probability"}), sorted(slide->getAuxImageNames()));
}

// Review Focus 1: a .bif that is not BIF reaches the BIF driver by name and is refused
// there with a useful message, rather than crashing or reporting "Cannot find driver".
TEST(BIFOpen, NotBifFails)
{
    TempFile file("bif");
    const std::string path = file.getPath().string();
    {
        TIFFKeeper tiff(path, false);
        TiffDirectory dir;
        dir.width = 64;
        dir.height = 64;
        dir.tiled = true;
        dir.tileWidth = 64;
        dir.tileHeight = 64;
        dir.channels = 3;
        dir.bitsPerSample = 8;
        dir.dataType = DataType::DT_Byte;
        dir.slideioCompression = Compression::Jpeg;
        dir.compressionQuality = 90;
        dir.description = "not a bif";
        TiffTools::setTags(tiff.getHandle(), dir);
        cv::Mat tile(64, 64, CV_8UC3, cv::Scalar(10, 20, 30));
        JpegEncodeParameters parameters(90);
        TiffTools::writeTile(tiff.getHandle(), 0, 0, Compression::Jpeg, tile, parameters);
        TiffTools::writeDirectory(tiff.getHandle());
    }
    try {
        openSlide(path, "AUTO");
        FAIL() << "a TIFF without iScan metadata opened as BIF";
    }
    catch (const RuntimeError& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("not a Ventana BIF file")) << e.what();
    }
}

// The aux scenes read their whole directory: tiled JPEG (OS-1 label), striped
// uncompressed (Ventana-1 label) and 8-bit grey LZW (Ventana-1 probability mask).
TEST(BIFCorpusOpen, AuxImagesRead)
{
    for (const std::string file : {"OS-1.bif", "Ventana-1.bif"}) {
        const std::string path = TestTools::getTestImagePath("bif/openslide", file);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        auto slide = ImageDriverManager::openSlide(path, "BIF");
        for (const auto& name : slide->getAuxImageNames()) {
            auto aux = slide->getAuxImage(name);
            const cv::Rect rect = aux->getRect();
            ASSERT_GT(rect.area(), 0) << file << " " << name;
            cv::Mat raster;
            aux->readBlock(rect, raster);
            EXPECT_EQ(rect.size(), raster.size()) << file << " " << name;
            EXPECT_EQ(aux->getNumChannels(), raster.channels()) << file << " " << name;
        }
    }
}
