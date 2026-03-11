#include <gtest/gtest.h>
#include <chrono>
#include <queue>
#include "tests/testlib/testtools.hpp"
#include "tests/testlib/testscene.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/convertertools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/base/rect.inl"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;
using namespace slideio::converter;

static uint8_t getChannelColor(int frame, int slice, int channel) {
    uint8_t color = 
        static_cast<uint8_t>(std::min(255, frame * 20 + slice * 5 + channel));
    return color;
}

static std::string getChannelName(int channel) {
    std::string name = "Channel_" + std::to_string(channel);
    return name;
}


class DummyScene : public TestScene
{
public:
	DummyScene() = default;
    std::string getChannelName(int channel) const override {
        return ::getChannelName(channel);
    }
    void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
        const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override {
        int numChannels = static_cast<int>(componentIndices.size());
        ASSERT_FALSE(componentIndices.empty());
        int cvType = (int)CV_MAKETYPE(static_cast<int>(getChannelDataType(0)), componentIndices.size());
        if (numChannels == 1) {
            uint8_t channelColor = getChannelColor(tFrameIndex, zSliceIndex, componentIndices[0]);
            output.create(blockSize, cvType);
            cv::Mat& mat = output.getMatRef();
            mat.setTo(cv::Scalar(channelColor));
        } else {
            std::vector<cv::Mat> channelMats;
            channelMats.reserve(numChannels);
            for (int i = 0; i < numChannels; ++i) {
                int channelIndex = componentIndices.empty() ? i : componentIndices[i];
                cv::Mat channelMat(blockSize, CV_MAKETYPE(static_cast<int>(getChannelDataType(0)), 1));
                channelMat.setTo(cv::Scalar(getChannelColor(tFrameIndex, zSliceIndex, channelIndex)));
                channelMats.push_back(channelMat);
            }
            cv::Mat mergedMat;
            cv::merge(channelMats, mergedMat);
            mergedMat.copyTo(output);
        }
    }
};


static std::shared_ptr<TestScene> makeScene(int channels = 3, int width = 512, int height = 512) {
    auto scene = std::make_shared<DummyScene>();
    scene->setNumChannels(channels);
    scene->setChannelDataType(DataType::DT_Byte);
    scene->setRect(cv::Rect(0, 0, width, height));
    scene->setResolution(Resolution(1e-3, 2e-3));
    scene->setMagnification(20.0);
    return scene;
}

class TestTiffConverter : public TiffConverter
{
public:
    virtual std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> cloneScene() const override {
		int channels = getScene()->getNumChannels();
        auto rect = getScene()->getRect();
        std::shared_ptr<TestScene> cvScene = makeScene(channels, rect.width, rect.height);
        cvScene->setNumTFrames(getScene()->getNumTFrames());
        cvScene->setNumZSlices(getScene()->getNumZSlices());
        cvScene->setRect(getScene()->getRect());
        std::shared_ptr<Scene> scene(new Scene(std::static_pointer_cast<CVScene>(cvScene)));
        return { nullptr, scene };
    }

};

static void configureCommonRanges(ConverterParameters& params, int channels, int zoomLevels) {
    params.setChannelRange(cv::Range(0, channels));
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 1));
    params.setRect(Rect(0, 0, 512, 512));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setTileWidth(128);
    tiffParams->setTileHeight(128);
    tiffParams->setNumZoomLevels(zoomLevels);
}

static void checkTiff(const TiffConverter& converter, 
        const std::string& filePath, const std::shared_ptr<CVScene>& scene) {
    std::vector<TiffDirectory> directories;
    TiffTools::scanFile(filePath, directories);
    const int numPages = converter.getNumTiffPages();
    const ConverterParameters& params = converter.getParameters();
    const std::shared_ptr<const TIFFContainerParameters>& tiffParameters =
        std::static_pointer_cast<const TIFFContainerParameters>(params.getContainerParameters());
	const int numZoomLevels = tiffParameters->getNumZoomLevels();
	const cv::Size tileSize(tiffParameters->getTileWidth(), tiffParameters->getTileHeight());
    EXPECT_EQ(numPages, static_cast<int>(directories.size()));
	const cv::Size imageSize = scene->getRect().size();
    for (int pageIndex=0; pageIndex<numPages; ++pageIndex) {
        const TiffPageStructure& pageStructure = converter.getTiffPage(pageIndex);
        const TiffDirectory& dir = directories[pageIndex];
        EXPECT_EQ(pageStructure.getChannelRange().size(), dir.channels);
        EXPECT_EQ(imageSize.width, dir.width);
        EXPECT_EQ(imageSize.height, dir.height);
        EXPECT_EQ(tileSize.width, dir.tileWidth);
		EXPECT_EQ(tileSize.height, dir.tileHeight);
		EXPECT_EQ(numZoomLevels-1, dir.subdirectories.size());
    }
}


static void testOMETIFFSubset(const ConverterParameters& params, 
    const std::shared_ptr<CVScene>& scene, bool interleaved3Channels = false) {
    slideio::TempFile tmp("ome.tiff");
    std::string outputPath = tmp.getPath().string();
    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }
    {
        TestTiffConverter converter;
        const cv::Range& channelRange = params.getChannelRange();
        const cv::Range& tframeRange = params.getTFrameRange();
        const cv::Range& zsliceRange = params.getSliceRange();
        auto tiffParams =
            std::static_pointer_cast<const TIFFContainerParameters>(params.getContainerParameters());
        int numZoomLevels = tiffParams->getNumZoomLevels();
        ASSERT_NO_THROW(converter.createFileLayout(scene, params));
		const int channelChunk = interleaved3Channels ? 3 : 1;
		const auto channelDirs = interleaved3Channels ? 1 : channelRange.size();
        const int numPages = zsliceRange.size() * tframeRange.size() * channelDirs;
        EXPECT_EQ(numPages, converter.getNumTiffPages());
        int sliceIndex = zsliceRange.start;
        int frameIndex = tframeRange.start;
        int channelIndex = channelRange.start;
        const int lastSlice = zsliceRange.end - 1;
        const int lastChannel = channelRange.end - 1;
        for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
            const auto& page = converter.getTiffPage(pageIndex);
            EXPECT_EQ(cv::Range(frameIndex, frameIndex + 1), page.getTFrameRange());
            EXPECT_EQ(cv::Range(sliceIndex, sliceIndex + 1), page.getZSliceRange());
            EXPECT_EQ(cv::Range(channelIndex, channelIndex + channelChunk), page.getChannelRange());
            EXPECT_EQ(frameIndex, page.getTFrameRange().start);
            EXPECT_EQ(sliceIndex, page.getZSliceRange().start);
            EXPECT_EQ(channelIndex, page.getChannelRange().start);
			EXPECT_EQ(channelChunk, page.getChannelRange().size());
            channelIndex += channelChunk;
            if (channelIndex > lastChannel) {
                channelIndex = channelRange.start;
                sliceIndex++;
                if (sliceIndex > lastSlice) {
                    channelIndex = channelRange.start;
                    sliceIndex = zsliceRange.start;
                    frameIndex++;
                }
            }
            EXPECT_EQ(numZoomLevels - 1, page.getNumSubDirectories());
        }
        converter.createTiff(outputPath, nullptr, 1);
        checkTiff(converter, outputPath, scene);
    }
}


TEST(TiffConverterTests, UninitializedConverter) {
    TiffConverter converter;
    EXPECT_EQ(0, converter.getNumTiffPages());
    EXPECT_THROW(converter.getTiffPage(0), RuntimeError);
    EXPECT_THROW(converter.createTiff("no-file", nullptr, 1), RuntimeError);
}

TEST(TiffConverterTests, CreateFileLayoutThrowsOnNullScene) {
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 1, 1);

    TiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(nullptr, params), RuntimeError);
}

TEST(TiffConverterTests, CreateFileLayoutThrowsOnInvalidParameters) {
    auto scene = makeScene(3);
    ConverterParameters params; // Default constructor creates invalid parameters

    TestTiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(scene, params), RuntimeError);
}

TEST(TiffConverterTests, CreateFileLayoutSVSSingleChannel) {
    auto scene = makeScene(1);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 1, 1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 1), page.getChannelRange());
    EXPECT_EQ(cv::Range(0, 1), page.getZSliceRange());
    EXPECT_EQ(cv::Range(0, 1), page.getTFrameRange());
}

TEST(TiffConverterTests, CreateFileLayoutSVSThreeChannels) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 3), page.getChannelRange());
}

TEST(TiffConverterTests, CreateFileLayoutSVSMultipleZoomLevels) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 3);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // SVS creates separate pages for each zoom level
    EXPECT_EQ(3, converter.getNumTiffPages());

    // Base level
    const auto& basePage = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 1), basePage.getZoomLevelRange());
    EXPECT_EQ(0, basePage.getNumSubDirectories());

    // Zoom level 1
    const auto& zoomPage1 = converter.getTiffPage(1);
    EXPECT_EQ(cv::Range(1, 2), zoomPage1.getZoomLevelRange());

    // Zoom level 2
    const auto& zoomPage2 = converter.getTiffPage(2);
    EXPECT_EQ(cv::Range(2, 3), zoomPage2.getZoomLevelRange());
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFSingleChannel) {
    auto scene = makeScene(1);
    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 1, 1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 1), page.getChannelRange());
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFThreeChannels) {
    auto scene = makeScene(3);
    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // OME-TIFF groups 3 channels into one page for JPEG
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 3), page.getChannelRange());
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFF2and4Channels) {
    auto scene = makeScene(2);
    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 2, 1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    scene->setNumChannels(4);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    scene->setNumChannels(3);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFMultipleZoomLevels) {
    auto scene = makeScene(3);
    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 3, 3);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // OME-TIFF uses subdirectories for zoom levels
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 1), page.getZoomLevelRange());
    EXPECT_EQ(2, page.getNumSubDirectories());

    const auto& subDir1 = page.getSubDirectory(0);
    EXPECT_EQ(cv::Range(1, 2), subDir1.getZoomLevelRange());

    const auto& subDir2 = page.getSubDirectory(1);
    EXPECT_EQ(cv::Range(2, 3), subDir2.getZoomLevelRange());
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFJpeg2000FourChannels) {
    auto scene = makeScene(4);
    OMETIFFJp2KConverterParameters params;
    configureCommonRanges(params, 4, 1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // JPEG2000 creates individual pages for each channel
    EXPECT_EQ(4, converter.getNumTiffPages());

    for (int i = 0; i < 4; ++i) {
        const auto& page = converter.getTiffPage(i);
        EXPECT_EQ(cv::Range(i, i + 1), page.getChannelRange());
    }
}

TEST(TiffConverterTests, CreateFileLayoutWithChannelSubset) {
    auto scene = makeScene(5);
    OMETIFFJp2KConverterParameters params;
    params.setChannelRange(cv::Range(1, 4)); // Select channels 1, 2, 3
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 1));
    cv::Rect sceneRect = scene->getRect();
    params.setRect(Rect(0, 0, sceneRect.width, sceneRect.height));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    EXPECT_EQ(3, converter.getNumTiffPages());
}

TEST(TiffConverterTests, CreateFileLayoutWithCustomRect) {
    auto scene = makeScene(3, 1024, 1024);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    Rect customRect(100, 100, 500, 500);
    params.setRect(customRect);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());
}

TEST(TiffConverterTests, CreateFileLayoutSVSThrowsOnMultipleSlices) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 3)); // Multiple slices
    params.setTFrameRange(cv::Range(0, 1));

    TestTiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(scene, params), RuntimeError);
}

TEST(TiffConverterTests, CreateFileLayoutSVSThrowsOnMultipleFrames) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 3)); // Multiple frames

    TestTiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(scene, params), RuntimeError);
}

TEST(TiffConverterTests, GetTiffPageThrowsOnInvalidIndex) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    TestTiffConverter converter;
    converter.createFileLayout(scene, params);

    EXPECT_NO_THROW(converter.getTiffPage(0));
    EXPECT_THROW(converter.getTiffPage(1), RuntimeError);
    EXPECT_THROW(converter.getTiffPage(-1), RuntimeError);
}

TEST(TiffConverterTests, MultipleCreateFileLayoutCallsResetState) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 2);

    TestTiffConverter converter;

    // First layout
    converter.createFileLayout(scene, params);
    EXPECT_EQ(2, converter.getNumTiffPages());

    // Second layout with different parameters
    configureCommonRanges(params, 3, 1);
    converter.createFileLayout(scene, params);
    EXPECT_EQ(1, converter.getNumTiffPages());
}

TEST(TiffConverterTests, OMETIFFLayoutFor3ByteChannels) {
    auto scene = makeScene(3);
    OMETIFFJp2KConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    cv::Rect sceneRect = scene->getRect();
    params.setRect(Rect(0, 0, sceneRect.width, sceneRect.height));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // Should create 2 slices * 3 frames = 6 pages
    EXPECT_EQ(6, converter.getNumTiffPages());

    // Each page should have planeCount = slices * frames = 2 * 3 = 6
    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(1, page.getPlaneCount());
}

TEST(TiffConverterTests, OMETIFFLayoutFor2ByteChannels) {
    auto scene = makeScene(3);
    OMETIFFJp2KConverterParameters params;
    params.setChannelRange(cv::Range(1, 3));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    cv::Rect sceneRect = scene->getRect();
    params.setRect(Rect(0, 0, sceneRect.width, sceneRect.height));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // Should create 2 channels * 2 slices * 3 frames = 12 pages
    EXPECT_EQ(12, converter.getNumTiffPages());

    // Each page should have planeCount = slices * frames = 2 * 3 = 6
    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(1, page.getPlaneCount());
}

TEST(TiffConverterTests, OMETIFFLayoutFor3_16BitChannels) {
    auto scene = makeScene(3);
    scene->setChannelDataType(DataType::DT_Int16);
    OMETIFFJp2KConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    cv::Rect sceneRect = scene->getRect();
    params.setRect(Rect(0, 0, sceneRect.width, sceneRect.height));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // Should create 2 slices * 3 frames = 18 pages
    EXPECT_EQ(6, converter.getNumTiffPages());

    // Each page should have planeCount = slices * frames = 2 * 3 = 6
    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(1, page.getPlaneCount());
}

TEST(TiffConverterTests, OMETIFFLayoutFor4ByteChannelsJpeg) {
    auto scene = makeScene(4);
    OMETIFFJp2KConverterParameters params;
    params.setChannelRange(cv::Range(0, 4));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    cv::Rect sceneRect = scene->getRect();
    params.setRect(Rect(0, 0, sceneRect.width, sceneRect.height));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    // Should create 4 channels * 2 slices * 3 frames = 24 pages
    EXPECT_EQ(24, converter.getNumTiffPages());

    // Each page should have planeCount = slices * frames = 2 * 3 = 6
    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(1, page.getPlaneCount());
}

TEST(TiffConverterTests, OMETIFFLayoutFor3_16BitChannelsJpeg) {
    auto scene = makeScene(3);
    scene->setChannelDataType(DataType::DT_Int16);
    OMETIFFJpegConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_THROW(converter.createFileLayout(scene, params), slideio::RuntimeError);
}

TEST(TiffConverterTests, ComputeCropRect) {
    auto scene = makeScene(4);
    scene->setRect(cv::Rect(1000, 1000, 20000, 40000));
    OMETIFFJp2KConverterParameters params;
    params.setRect(Rect(10000, 5000, 10000, 15000));
    params.setChannelRange(cv::Range(0, 4));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    cv::Rect rect = converter.getSceneRect();
    EXPECT_EQ(cv::Rect(10000, 5000, 10000, 15000), rect);
}

TEST(TiffConverterTests, ComputeCropRectInvalid) {
    auto scene = makeScene(4);
    scene->setRect(cv::Rect(1000, 1000, 20000, 40000));
    OMETIFFJp2KConverterParameters params;
    params.setRect(Rect(10000, 5000, 10000, 55000));
    params.setChannelRange(cv::Range(0, 4));
    params.setSliceRange(cv::Range(0, 2));
    params.setTFrameRange(cv::Range(0, 3));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TestTiffConverter converter;
    ASSERT_THROW(converter.createFileLayout(scene, params), slideio::RuntimeError);

    params.setRect(Rect(0, 0, 20000, 40000));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));

    params.setRect(Rect(1, 0, 20000, 40000));
    ASSERT_THROW(converter.createFileLayout(scene, params), slideio::RuntimeError);

    params.setRect(Rect(0, 1, 20000, 40000));
    ASSERT_THROW(converter.createFileLayout(scene, params), slideio::RuntimeError);

    params.setRect(Rect(0, 0, 20000, 40000));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
}

TEST(TiffConverterTests, ComputeNumberOfTiles) {
    auto scene = makeScene(1);
    scene->setRect(cv::Rect(1000, 1000, 20000, 40000));
    OMETIFFJp2KConverterParameters params;
    params.setRect(Rect(10000, 5000, 10000, 20000));
    params.setChannelRange(cv::Range(0, 1));
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 1));
    constexpr int tileWidth = 100;
    constexpr int tileHeight = 200;
    params.setTileWidth(tileWidth);
    params.setTileHeight(tileHeight);
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);
    Rect rect = params.getRect();
    int tilesX = (rect.width + params.getTileWidth() - 1) / params.getTileWidth();
    int tilesY = (rect.height + params.getTileHeight() - 1) / params.getTileHeight();
    int numTiles = tilesX * tilesY;
    TestTiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    int totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
    params.setSliceRange(cv::Range(0, 2));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numTiles *= 2;
    totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
    params.setChannelRange(cv::Range(0, 2));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numTiles *= 2;
    totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
    params.setTFrameRange(cv::Range(0, 2));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numTiles *= 2;
    totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
    params.setRect(Rect(10000, 5000, 10000 - 50, 20000 - 50));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
    Rect blockRect = params.getRect();
    params.setChannelRange(cv::Range(0, 1));
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 1));
    tilesX = (blockRect.width + params.getTileWidth() - 1) / params.getTileWidth();
    tilesY = (blockRect.height + params.getTileHeight() - 1) / params.getTileHeight();
    numTiles = tilesX * tilesY;
    Rect sceneRect = params.getRect();
    cv::Rect zoomLevelRect = ConverterTools::computeZoomLevelRect(cv::Rect(0, 0, sceneRect.width, sceneRect.height),
                                                                  cv::Size(tileWidth, tileHeight), 1);
    tilesX = (zoomLevelRect.width + params.getTileWidth() - 1) / params.getTileWidth();
    tilesY = (zoomLevelRect.height + params.getTileHeight() - 1) / params.getTileHeight();
    numTiles += tilesX * tilesY;
    tiffParams->setNumZoomLevels(2);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
}

TEST(TiffConverterTests, OMETIFFDefaultSettings) {
    constexpr int numChannels = 4;
    constexpr int numTFrames = 3;
    constexpr int numSlices = 5;
    const cv::Rect sceneRect = cv::Rect(0, 0, 1512, 2512);
    auto scene = std::make_shared<DummyScene>();
    scene->setNumChannels(numChannels);
    scene->setChannelDataType(DataType::DT_Byte);
    scene->setRect(sceneRect);
    scene->setNumZSlices(numSlices);
    scene->setNumTFrames(numTFrames);
    scene->setResolution(Resolution(1e-6, 1e-6));
    scene->setMagnification(20.0);
    OMETIFFJp2KConverterParameters params;
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(60, converter.getNumTiffPages());
    tiffParams->setNumZoomLevels(2);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(60, converter.getNumTiffPages());
}


TEST(TiffConverterTests, CreateFileLayoutOMETIFFSubset) {
    constexpr int numSlices = 3;
    constexpr int numFrames = 5;
    constexpr int numZoomLevels = 2;
    constexpr int numChannels = 5;
    const cv::Rect sceneRect(0, 0, 4096, 2048);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);
    OMETIFFJp2KConverterParameters params;

    TestTiffConverter converter;
    // The whole dataset
    params.setTFrameRange(cv::Range(0, numFrames));
    params.setSliceRange(cv::Range(0, numSlices));
    params.setChannelRange(cv::Range(0, numChannels));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(numZoomLevels);
    testOMETIFFSubset(params, scene);
    // Slice subset
    params.setSliceRange(cv::Range(1, 3));
    testOMETIFFSubset(params, scene);
    // T frame subset
    params.setTFrameRange(cv::Range(2, 4));
    params.setSliceRange(cv::Range(0, numSlices));
    testOMETIFFSubset(params, scene);
    // T frame and slice subset
    params.setTFrameRange(cv::Range(2, 4));
    params.setSliceRange(cv::Range(1, 3));
    testOMETIFFSubset(params, scene);
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFJpegSubset) {
    constexpr int numSlices = 3;
    constexpr int numFrames = 5;
    constexpr int numZoomLevels = 2;
    constexpr int numChannels = 3;
    const cv::Rect sceneRect(0, 0, 4096, 2048);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);
    OMETIFFJpegConverterParameters params;

    TestTiffConverter converter;
    // The whole dataset
    params.setTFrameRange(cv::Range(0, numFrames));
    params.setSliceRange(cv::Range(0, numSlices));
    params.setChannelRange(cv::Range(0, numChannels));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(numZoomLevels);
    testOMETIFFSubset(params, scene, true);
    // Slice subset
    params.setSliceRange(cv::Range(1, 3));
    testOMETIFFSubset(params, scene, true);
    // T frame subset
    params.setTFrameRange(cv::Range(2, 4));
    params.setSliceRange(cv::Range(0, numSlices));
    testOMETIFFSubset(params, scene, true);
    // T frame and slice subset
    params.setTFrameRange(cv::Range(2, 4));
    params.setSliceRange(cv::Range(1, 3));
    testOMETIFFSubset(params, scene, true);
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFChannelSubset) {
    constexpr int numSlices = 3;
    constexpr int numFrames = 5;
    constexpr int numZoomLevels = 5;
    constexpr int numChannels = 5;
    const cv::Rect sceneRect(0, 0, 4096, 8192);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);
    OMETIFFJp2KConverterParameters params;

    TestTiffConverter converter;
    // All channels
    params.setTFrameRange(cv::Range(2, 4));
    params.setSliceRange(cv::Range(1, 3));
    params.setChannelRange(cv::Range(0, numChannels));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(numZoomLevels);
    
    auto start = std::chrono::high_resolution_clock::now();
    testOMETIFFSubset(params, scene);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "testOMETIFFSubset execution time: " << elapsed.count() << " seconds" << std::endl;
    
    // Channel subset
    params.setChannelRange(cv::Range(2, 4));
    testOMETIFFSubset(params, scene);
}

TEST(TiffConverterTests, SVSDefaultSettings) {
    constexpr int numChannels = 3;
    constexpr int numTFrames = 1;
    constexpr int numSlices = 1;
    const cv::Rect sceneRect = cv::Rect(0, 0, 1512, 2512);
    auto scene = std::make_shared<DummyScene>();
    scene->setNumChannels(numChannels);
    scene->setChannelDataType(DataType::DT_Byte);
    scene->setRect(sceneRect);
    scene->setNumZSlices(numSlices);
    scene->setNumTFrames(numTFrames);
    scene->setResolution(Resolution(1e-6, 1e-6));
    scene->setMagnification(20.0);
    SVSJpegConverterParameters params;
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());
    tiffParams->setNumZoomLevels(5);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(5, converter.getNumTiffPages());
}

TEST(TiffConverterTests, jpegToOMETIFF) {
    constexpr int tileWidth = 512;
    constexpr int tileHeight = 128;
    constexpr int numZoomLevels = 5;

    std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SlidePtr slide = openSlide(path, "GDAL");
    ScenePtr scene = slide->getScene(0);
    auto sceneRect = scene->getRect();
    const int sceneWidth = std::get<2>(sceneRect);
    const int sceneHeight = std::get<3>(sceneRect);
    const int numChannels = scene->getNumChannels();
    const DataType dt = scene->getChannelDataType(0);
    ASSERT_TRUE(scene.get() != nullptr);

    slideio::TempFile tmp("ome.tiff");
    std::string outputPath = tmp.getPath().string();
    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }
    OMETIFFJpegConverterParameters parameters;
    auto tiffParams =
        std::static_pointer_cast<TIFFContainerParameters>(parameters.getContainerParameters());
    parameters.setQuality(99);
    tiffParams->setNumZoomLevels(numZoomLevels);
    tiffParams->setTileWidth(tileWidth);
    tiffParams->setTileHeight(tileHeight);

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene->getCVScene(), parameters));
    EXPECT_EQ(1, converter.getNumTiffPages());
    std::set<int> progress;
    int progressLast = 0;
    ASSERT_NO_THROW(converter.createTiff(outputPath,
        [&progress, &progressLast](int pr) { progressLast = pr; progress.insert(pr); }, 1));
    EXPECT_EQ(100, progressLast);
    EXPECT_GT(progress.size(), 10);
    std::vector<TiffDirectory> directories;
    TiffTools::scanFile(outputPath, directories);
    EXPECT_EQ(1, directories.size());
    const TiffDirectory& baseDir = directories[0];
    EXPECT_EQ(4, baseDir.subdirectories.size());

    EXPECT_EQ(sceneWidth, baseDir.width);
    EXPECT_EQ(sceneHeight, baseDir.height);
    EXPECT_EQ(numChannels, baseDir.channels);
    EXPECT_EQ(tileWidth, baseDir.tileWidth);
    EXPECT_EQ(tileHeight, baseDir.tileHeight);
    EXPECT_EQ(Compression::Jpeg, baseDir.slideioCompression);
    EXPECT_EQ(dt, baseDir.dataType);

    std::vector<cv::Size> dirSizes = {
        cv::Size(sceneWidth / 2, sceneHeight / 2),
        cv::Size(sceneWidth / 4, sceneHeight / 4),
        cv::Size(sceneWidth / 8, sceneHeight / 8),
        cv::Size(sceneWidth / 16, sceneHeight / 16)
    };
    int dirIndex = 0;
    for (const auto& dir : baseDir.subdirectories) {
        EXPECT_EQ(dirSizes[dirIndex].width, dir.width);
        EXPECT_EQ(dirSizes[dirIndex].height, dir.height);
        EXPECT_EQ(numChannels, dir.channels);
        EXPECT_EQ(tileWidth, dir.tileWidth);
        EXPECT_EQ(tileHeight, dir.tileHeight);
        EXPECT_EQ(Compression::Jpeg, dir.slideioCompression);
        EXPECT_EQ(dt, dir.dataType);
        ++dirIndex;
    }
}

TEST(TiffConverterTests, jpegToSVS) {
    constexpr int tileWidth = 512;
    constexpr int tileHeight = 128;
    constexpr int numZoomLevels = 5;

    std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SlidePtr slide = openSlide(path, "GDAL");
    ScenePtr scene = slide->getScene(0);
    auto sceneRect = scene->getRect();
    const int sceneWidth = std::get<2>(sceneRect);
    const int sceneHeight = std::get<3>(sceneRect);
    const int numChannels = scene->getNumChannels();
    const DataType dt = scene->getChannelDataType(0);
    ASSERT_TRUE(scene.get() != nullptr);

    slideio::TempFile tmp("svs");
    std::string outputPath = tmp.getPath().string();
    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }
    SVSJpegConverterParameters parameters;
    auto tiffParams =
        std::static_pointer_cast<TIFFContainerParameters>(parameters.getContainerParameters());
    parameters.setQuality(99);
    tiffParams->setNumZoomLevels(numZoomLevels);
    tiffParams->setTileWidth(tileWidth);
    tiffParams->setTileHeight(tileHeight);

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene->getCVScene(), parameters));
    EXPECT_EQ(5, converter.getNumTiffPages());
    ASSERT_NO_THROW(converter.createTiff(outputPath, nullptr, 1));
    std::vector<TiffDirectory> directories;
    TiffTools::scanFile(outputPath, directories);
    EXPECT_EQ(5, directories.size());

    std::vector<cv::Size> dirSizes = {
        cv::Size(sceneWidth, sceneHeight),
        cv::Size(sceneWidth / 2, sceneHeight / 2),
        cv::Size(sceneWidth / 4, sceneHeight / 4),
        cv::Size(sceneWidth / 8, sceneHeight / 8),
        cv::Size(sceneWidth / 16, sceneHeight / 16)
    };
    int dirIndex = 0;
    for (const auto& dir : directories) {
        EXPECT_EQ(dirSizes[dirIndex].width, dir.width);
        EXPECT_EQ(dirSizes[dirIndex].height, dir.height);
        EXPECT_EQ(numChannels, dir.channels);
        EXPECT_EQ(tileWidth, dir.tileWidth);
        EXPECT_EQ(tileHeight, dir.tileHeight);
        EXPECT_EQ(Compression::Jpeg, dir.slideioCompression);
        EXPECT_EQ(dt, dir.dataType);
        ++dirIndex;
    }
}

TEST(TiffConverterTests, OMETIFFJp2KRaster) {

    slideio::TempFile tmp("ome.tiff");
    std::string outputPath = tmp.getPath().string();
    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }

    constexpr int numSlices = 5;
    constexpr int numFrames = 3;
    constexpr int numZoomLevels = 2;
    constexpr int numChannels = 5;
    const cv::Rect sceneRect(0, 0, 4096, 2048);
    const cv::Size tileSize(128, 512);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);
    OMETIFFJp2KConverterParameters params;
    params.setCompressionRate(1);

    const cv::Range sliceRange(1, 3);
    const cv::Range frameRange(2, 4);
    const cv::Range channelRange(2, 4);
    params.setChannelRange(channelRange);
    params.setTFrameRange(frameRange);
    params.setSliceRange(sliceRange);
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(numZoomLevels);
    tiffParams->setTileHeight(tileSize.height);
	tiffParams->setTileWidth(tileSize.width);
    TestTiffConverter converter;
    converter.createFileLayout(scene, params);
    converter.createTiff(outputPath, nullptr, 1);
    auto slide = openSlide(outputPath, "OMETIFF");
	ASSERT_EQ(1, slide->getNumScenes());
	auto cvScene = slide->getScene(0)->getCVScene();
	Rect cvSceneRect = cvScene->getRect();
    EXPECT_EQ(converter.getSceneRect(), cvSceneRect);
    EXPECT_EQ(channelRange.size(), cvScene->getNumChannels());
    EXPECT_EQ(scene->getChannelDataType(0), cvScene->getChannelDataType(0));
    EXPECT_EQ(sliceRange.size(), cvScene->getNumZSlices());
	EXPECT_EQ(frameRange.size(), cvScene->getNumTFrames());
    for (int channel = 0; channel<channelRange.size(); ++channel) {
		EXPECT_EQ(getChannelName(channel + channelRange.start), cvScene->getChannelName(channel));
    }
	cv::Rect blockRect = cv::Rect(0,0,256,256);
    for (int frame=0; frame<frameRange.size(); ++frame) {
        for (int slice=0; slice<sliceRange.size(); ++slice) {
            for (int channel=0; channel<channelRange.size(); ++channel) {
                cv::Mat raster;
                cvScene->read4DBlockChannels(blockRect, 
                    { channel },
                    { slice, slice + 1 },
                    { frame, frame + 1 },
                    raster);
                
                ASSERT_FALSE(raster.empty());
                EXPECT_EQ(blockRect.width, raster.cols);
                EXPECT_EQ(blockRect.height, raster.rows);
                EXPECT_EQ(CV_8UC1, raster.type());
                
                double minVal, maxVal;
                cv::minMaxLoc(raster, &minVal, &maxVal);
                
                int sourceFrame = frame + frameRange.start;
                int sourceSlice = slice + sliceRange.start;
                int sourceChannel = channel + channelRange.start;
                uint8_t expectedValue = getChannelColor(sourceFrame, sourceSlice, sourceChannel);
                
                EXPECT_EQ(expectedValue, static_cast<uint8_t>(minVal));
                EXPECT_EQ(expectedValue, static_cast<uint8_t>(maxVal));
            }
		}
    }

    EXPECT_EQ(numZoomLevels, cvScene->getNumZoomLevels());
    double expectedScale = 1.;
    for (int level=0; level<numZoomLevels; ++level) {
        const LevelInfo* levelInfo = cvScene->getZoomLevelInfo(level);
        const double levelScale = levelInfo->getScale();
		EXPECT_DOUBLE_EQ(expectedScale, levelScale);
        EXPECT_EQ(Size(tileSize.width, tileSize.height), levelInfo->getTileSize());
		expectedScale /= 2.;
	}
    EXPECT_DOUBLE_EQ(scene->getMagnification(), cvScene->getMagnification());
    EXPECT_NEAR(scene->getResolution().x, cvScene->getResolution().x, 1.e-5);
    EXPECT_NEAR(scene->getResolution().y, cvScene->getResolution().y, 1.e-5);
}

TEST(TiffConverterTests, OMETIFFJpegRaster) {

    slideio::TempFile tmp("ome.tiff");
    std::string outputPath = tmp.getPath().string();
    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }

    constexpr int numSlices = 5;
    constexpr int numFrames = 3;
    constexpr int numZoomLevels = 2;
    constexpr int numChannels = 3;
    const cv::Rect sceneRect(0, 0, 4096, 2048);
    const cv::Size tileSize(128, 512);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);
    OMETIFFJp2KConverterParameters params;
    params.setCompressionRate(1);

    const cv::Range sliceRange(1, 3);
    const cv::Range frameRange(2, 4);
    const cv::Range channelRange(0, numChannels);
    params.setChannelRange(channelRange);
    params.setTFrameRange(frameRange);
    params.setSliceRange(sliceRange);
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(numZoomLevels);
	tiffParams->setTileHeight(tileSize.height);
	tiffParams->setTileWidth(tileSize.width);
    TestTiffConverter converter;
    converter.createFileLayout(scene, params);
    converter.createTiff(outputPath, nullptr, 1);
    auto slide = openSlide(outputPath, "OMETIFF");
    ASSERT_EQ(1, slide->getNumScenes());
    auto cvScene = slide->getScene(0)->getCVScene();
    EXPECT_EQ(converter.getSceneRect(), cvScene->getRect());
    EXPECT_EQ(channelRange.size(), cvScene->getNumChannels());
    EXPECT_EQ(scene->getChannelDataType(0), cvScene->getChannelDataType(0));
    EXPECT_EQ(sliceRange.size(), cvScene->getNumZSlices());
    EXPECT_EQ(frameRange.size(), cvScene->getNumTFrames());
    for (int channel = 0; channel < channelRange.size(); ++channel) {
        EXPECT_EQ(getChannelName(channel + channelRange.start), cvScene->getChannelName(channel));
    }
    cv::Rect blockRect = cv::Rect(0, 0, 256, 256);
    for (int frame = 0; frame < frameRange.size(); ++frame) {
        for (int slice = 0; slice < sliceRange.size(); ++slice) {
            for (int channel = 0; channel < channelRange.size(); ++channel) {
                cv::Mat raster;
                cvScene->read4DBlockChannels(blockRect, { channel }, { slice, slice + 1 }, { frame, frame + 1 }, raster);

                ASSERT_FALSE(raster.empty());
                EXPECT_EQ(blockRect.width, raster.cols);
                EXPECT_EQ(blockRect.height, raster.rows);
                EXPECT_EQ(CV_8UC1, raster.type());

                double minVal, maxVal;
                cv::minMaxLoc(raster, &minVal, &maxVal);

                int sourceFrame = frame + frameRange.start;
                int sourceSlice = slice + sliceRange.start;
                int sourceChannel = channel + channelRange.start;
                uint8_t expectedValue = getChannelColor(sourceFrame, sourceSlice, sourceChannel);

                EXPECT_EQ(expectedValue, static_cast<uint8_t>(minVal));
                EXPECT_EQ(expectedValue, static_cast<uint8_t>(maxVal));
            }
        }
    }

    EXPECT_EQ(numZoomLevels, cvScene->getNumZoomLevels());
    double expectedScale = 1.;
    for (int level = 0; level < numZoomLevels; ++level) {
        const LevelInfo* levelInfo = cvScene->getZoomLevelInfo(level);
        const double levelScale = levelInfo->getScale();
        EXPECT_DOUBLE_EQ(expectedScale, levelScale);
        EXPECT_EQ(Size(tileSize.width, tileSize.height), levelInfo->getTileSize());
        expectedScale /= 2.;
    }
    EXPECT_DOUBLE_EQ(scene->getMagnification(), cvScene->getMagnification());
    EXPECT_NEAR(scene->getResolution().x, cvScene->getResolution().x, 1.e-5);
	EXPECT_NEAR(scene->getResolution().y, cvScene->getResolution().y, 1.e-5);
	EXPECT_EQ(scene->getName(), cvScene->getName());
}

// Helper to set up a TiffConverter with a given crop rect for createTileQueue tests.
static TiffConverter setupConverterForTileQueue(int imageWidth, int imageHeight, int tileWidth, int tileHeight,
	int numChannels = 1, int numZoomLevels = 1) {
	auto scene = makeScene(numChannels, imageWidth, imageHeight);
	OMETIFFJp2KConverterParameters params;
	params.setChannelRange(cv::Range(0, numChannels));
	params.setSliceRange(cv::Range(0, 1));
	params.setTFrameRange(cv::Range(0, 1));
	params.setRect(Rect(0, 0, imageWidth, imageHeight));
	auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
	tiffParams->setTileWidth(tileWidth);
	tiffParams->setTileHeight(tileHeight);
	tiffParams->setNumZoomLevels(numZoomLevels);

	TestTiffConverter converter;
	converter.createFileLayout(scene, params);
	return converter;
}

TEST(TiffConverterTests, CreateTileQueue_SingleTile) {
	TiffConverter converter = setupConverterForTileQueue(128, 128, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	ASSERT_EQ(1u, queue.size());
	auto block = queue.front();
	EXPECT_EQ(cv::Rect(0, 0, 128, 128), block.rect);
	EXPECT_EQ(0u, block.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_MultipleTilesBatch1) {
	TiffConverter converter = setupConverterForTileQueue(512, 256, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	// 4 cols x 2 rows = 8 blocks
	ASSERT_EQ(8u, queue.size());
	size_t expectedSeqId = 0;
	for (int row = 0; row < 2; ++row) {
		for (int col = 0; col < 4; ++col) {
			auto block = queue.front();
			queue.pop();
			EXPECT_EQ(cv::Rect(col * 128, row * 128, 128, 128), block.rect);
			EXPECT_EQ(expectedSeqId, block.firstTileSequenceId);
			expectedSeqId += 1;
		}
	}
}

TEST(TiffConverterTests, CreateTileQueue_MultipleTilesLargeBatch) {
	TiffConverter converter = setupConverterForTileQueue(512, 256, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 4, queue);

	// batch=4 covers 4*128=512 = full width, so 1 block per row, 2 rows
	ASSERT_EQ(2u, queue.size());
	auto block0 = queue.front();
	queue.pop();
	EXPECT_EQ(cv::Rect(0, 0, 512, 128), block0.rect);
	EXPECT_EQ(0u, block0.firstTileSequenceId);

	auto block1 = queue.front();
	queue.pop();
	EXPECT_EQ(cv::Rect(0, 128, 512, 128), block1.rect);
	EXPECT_EQ(4u, block1.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_BatchSizeClamped) {
	TiffConverter converter = setupConverterForTileQueue(256, 256, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	// batch=0 should be clamped to 1
	std::queue<TiffConverter::Block> queue0;
	converter.createTileQueue(dir, page, 0, queue0);

	std::queue<TiffConverter::Block> queue1;
	converter.createTileQueue(dir, page, 1, queue1);

	ASSERT_EQ(queue0.size(), queue1.size());
	while (!queue0.empty()) {
		EXPECT_EQ(queue0.front().rect, queue1.front().rect);
		EXPECT_EQ(queue0.front().firstTileSequenceId, queue1.front().firstTileSequenceId);
		queue0.pop();
		queue1.pop();
	}

	// batch=-1 should also be clamped to 1
	std::queue<TiffConverter::Block> queueNeg;
	converter.createTileQueue(dir, page, -1, queueNeg);
	ASSERT_EQ(4u, queueNeg.size()); // 2x2 = 4 blocks with batch=1
}

TEST(TiffConverterTests, CreateTileQueue_NonExactBoundaries) {
	// 500x500 with 128x128 tiles: ceil(500/128) = 4 in each dimension
	TiffConverter converter = setupConverterForTileQueue(500, 500, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	// 4 cols x 4 rows = 16 blocks
	ASSERT_EQ(16u, queue.size());

	// Verify sequence IDs are contiguous 0..15
	for (size_t i = 0; i < 16; ++i) {
		auto block = queue.front();
		queue.pop();
		EXPECT_EQ(i, block.firstTileSequenceId);
		EXPECT_EQ(128, block.rect.width);
		EXPECT_EQ(128, block.rect.height);
	}
}

TEST(TiffConverterTests, CreateTileQueue_ZoomLevel1) {
	// 512x512 scene, tile 128x128, zoom level 1
	// sceneTileSize = scaleSize(128x128, 1, false) = 256x256
	// So 2 cols x 2 rows = 4 blocks
	TiffConverter converter = setupConverterForTileQueue(512, 512, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(1, 2));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	ASSERT_EQ(4u, queue.size());

	auto b0 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(0, 0, 256, 256), b0.rect);
	EXPECT_EQ(0u, b0.firstTileSequenceId);

	auto b1 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(256, 0, 256, 256), b1.rect);
	EXPECT_EQ(1u, b1.firstTileSequenceId);

	auto b2 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(0, 256, 256, 256), b2.rect);
	EXPECT_EQ(2u, b2.firstTileSequenceId);

	auto b3 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(256, 256, 256, 256), b3.rect);
	EXPECT_EQ(3u, b3.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_BatchSize2) {
	// 512x256, tile 128x128, batch=2
	// batchWidth = 2*128 = 256
	// 2 batches per row (512/256), 2 rows (256/128)
	TiffConverter converter = setupConverterForTileQueue(512, 256, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 2, queue);

	ASSERT_EQ(4u, queue.size());

	auto b0 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(0, 0, 256, 128), b0.rect);
	EXPECT_EQ(0u, b0.firstTileSequenceId);

	auto b1 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(256, 0, 256, 128), b1.rect);
	EXPECT_EQ(2u, b1.firstTileSequenceId);

	auto b2 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(0, 128, 256, 128), b2.rect);
	EXPECT_EQ(4u, b2.firstTileSequenceId);

	auto b3 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(256, 128, 256, 128), b3.rect);
	EXPECT_EQ(6u, b3.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_BatchLargerThanRow) {
	// 256x128, tile 128x128, batch=10 (larger than columns)
	// Only 2 tiles per row, so batch is effectively 2
	TiffConverter converter = setupConverterForTileQueue(256, 128, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 10, queue);

	// One block covering the full row
	ASSERT_EQ(1u, queue.size());
	auto block = queue.front();
	EXPECT_EQ(cv::Rect(0, 0, 256, 128), block.rect);
	EXPECT_EQ(0u, block.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_TotalTileCount) {
	// 1000x800, tile 256x256, batch=1
	// cols = ceil(1000/256) = 4, rows = ceil(800/256) = 4 => 16 tiles
	TiffConverter converter = setupConverterForTileQueue(1000, 800, 256, 256);

	TiffDirectory dir;
	dir.tileWidth = 256;
	dir.tileHeight = 256;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	// Count total tiles by summing up tiles per block
	size_t totalTiles = 0;
	size_t lastSeqId = 0;
	bool first = true;
	while (!queue.empty()) {
		auto block = queue.front();
		queue.pop();
		size_t numTilesInBlock = block.rect.width / 256;
		if (first) {
			EXPECT_EQ(0u, block.firstTileSequenceId);
			first = false;
		}
		totalTiles += numTilesInBlock;
		lastSeqId = block.firstTileSequenceId + numTilesInBlock;
	}
	EXPECT_EQ(16u, totalTiles);
	EXPECT_EQ(16u, lastSeqId);
}

TEST(TiffConverterTests, CreateTileQueue_WithCropRectOffset) {
	// Scene 1024x1024, crop rect offset (100, 200, 512, 256)
	auto scene = makeScene(1, 1024, 1024);
	OMETIFFJp2KConverterParameters params;
	params.setChannelRange(cv::Range(0, 1));
	params.setSliceRange(cv::Range(0, 1));
	params.setTFrameRange(cv::Range(0, 1));
	params.setRect(Rect(100, 200, 512, 256));
	auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
	tiffParams->setTileWidth(128);
	tiffParams->setTileHeight(128);
	tiffParams->setNumZoomLevels(1);

	TestTiffConverter converter;
	converter.createFileLayout(scene, params);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	// 4 cols x 2 rows = 8 blocks, starting from (100, 200)
	ASSERT_EQ(8u, queue.size());

	auto b0 = queue.front(); queue.pop();
	EXPECT_EQ(100, b0.rect.x);
	EXPECT_EQ(200, b0.rect.y);
	EXPECT_EQ(128, b0.rect.width);
	EXPECT_EQ(128, b0.rect.height);
	EXPECT_EQ(0u, b0.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_RectangularTiles) {
	// 512x512 image with non-square tiles 256x128
	TiffConverter converter = setupConverterForTileQueue(512, 512, 256, 128);

	TiffDirectory dir;
	dir.tileWidth = 256;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 1, queue);

	// cols = 512/256 = 2, rows = 512/128 = 4 => 8 blocks
	ASSERT_EQ(8u, queue.size());

	auto b0 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(0, 0, 256, 128), b0.rect);
	EXPECT_EQ(0u, b0.firstTileSequenceId);

	auto b1 = queue.front(); queue.pop();
	EXPECT_EQ(cv::Rect(256, 0, 256, 128), b1.rect);
	EXPECT_EQ(1u, b1.firstTileSequenceId);
}

TEST(TiffConverterTests, CreateTileQueue_SequenceIdsMonotonic) {
	// Verify sequence IDs are strictly monotonically increasing
	TiffConverter converter = setupConverterForTileQueue(1024, 768, 128, 128);

	TiffDirectory dir;
	dir.tileWidth = 128;
	dir.tileHeight = 128;
	TiffDirectoryStructure page;
	page.setZoomLevelRange(cv::Range(0, 1));

	std::queue<TiffConverter::Block> queue;
	converter.createTileQueue(dir, page, 3, queue);

	ASSERT_FALSE(queue.empty());
	size_t prevSeqId = 0;
	bool first = true;
	while (!queue.empty()) {
		auto block = queue.front();
		queue.pop();
		if (first) {
			EXPECT_EQ(0u, block.firstTileSequenceId);
			first = false;
		} else {
			EXPECT_GT(block.firstTileSequenceId, prevSeqId);
		}
		prevSeqId = block.firstTileSequenceId;
	}
}
