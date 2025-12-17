#include <gtest/gtest.h>
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

using namespace slideio;
using namespace slideio::converter;

namespace {
class DummyScene : public TestScene {
public:
    void readResampledBlockChannelsEx(const cv::Rect&, const cv::Size&, const std::vector<int>&,
        int, int, cv::OutputArray) override {}
};

std::shared_ptr<TestScene> makeScene(int channels = 3, int width = 512, int height = 512) {
    auto scene = std::make_shared<DummyScene>();
    scene->setNumChannels(channels);
    scene->setChannelDataType(DataType::DT_Byte);
    scene->setRect(cv::Rect(0, 0, width, height));
    scene->setResolution(Resolution(1e-6, 1e-6));
    scene->setMagnification(20.0);
    return scene;
}

void configureCommonRanges(ConverterParameters& params, int channels, int zoomLevels) {
    params.setChannelRange(cv::Range(0, channels));
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 1));
    params.setRect(Rect(0, 0, 512, 512));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setTileWidth(128);
    tiffParams->setTileHeight(128);
    tiffParams->setNumZoomLevels(zoomLevels);
}
}

TEST(TiffConverterTests, UninitializedConverter) {
    TiffConverter converter;
    EXPECT_EQ(0, converter.getNumTiffPages());
    EXPECT_THROW(converter.getTiffPage(0), RuntimeError);
    EXPECT_THROW(converter.createTiff("no-file", nullptr), RuntimeError);
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

    TiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(scene, params), RuntimeError);
}

TEST(TiffConverterTests, CreateFileLayoutSVSSingleChannel) {
    auto scene = makeScene(1);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 1, 1);

    TiffConverter converter;
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

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 3), page.getChannelRange());
}

TEST(TiffConverterTests, CreateFileLayoutSVSMultipleZoomLevels) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 3);

    TiffConverter converter;
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

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());

    const auto& page = converter.getTiffPage(0);
    EXPECT_EQ(cv::Range(0, 1), page.getChannelRange());
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFThreeChannels) {
    auto scene = makeScene(3);
    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    TiffConverter converter;
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

    TiffConverter converter;
    ASSERT_THROW(converter.createFileLayout(scene, params), slideio::RuntimeError);

    scene->setNumChannels(4);
    ASSERT_THROW(converter.createFileLayout(scene, params), slideio::RuntimeError);

    scene->setNumChannels(3);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFMultipleZoomLevels) {
    auto scene = makeScene(3);
    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 3, 3);

    TiffConverter converter;
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

    TiffConverter converter;
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

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    
    EXPECT_EQ(3, converter.getNumTiffPages());
}

TEST(TiffConverterTests, CreateFileLayoutWithCustomRect) {
    auto scene = makeScene(3, 1024, 1024);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);
    
    Rect customRect(100, 100, 500, 500);
    params.setRect(customRect);

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    EXPECT_EQ(1, converter.getNumTiffPages());
}

TEST(TiffConverterTests, CreateFileLayoutSVSThrowsOnMultipleSlices) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 3)); // Multiple slices
    params.setTFrameRange(cv::Range(0, 1));

    TiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(scene, params), RuntimeError);
}

TEST(TiffConverterTests, CreateFileLayoutSVSThrowsOnMultipleFrames) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    params.setChannelRange(cv::Range(0, 3));
    params.setSliceRange(cv::Range(0, 1));
    params.setTFrameRange(cv::Range(0, 3)); // Multiple frames

    TiffConverter converter;
    EXPECT_THROW(converter.createFileLayout(scene, params), RuntimeError);
}

TEST(TiffConverterTests, GetTiffPageThrowsOnInvalidIndex) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    TiffConverter converter;
    converter.createFileLayout(scene, params);
    
    EXPECT_NO_THROW(converter.getTiffPage(0));
    EXPECT_THROW(converter.getTiffPage(1), RuntimeError);
    EXPECT_THROW(converter.getTiffPage(-1), RuntimeError);
}

TEST(TiffConverterTests, MultipleCreateFileLayoutCallsResetState) {
    auto scene = makeScene(3);
    SVSJpegConverterParameters params;
    configureCommonRanges(params, 3, 2);

    TiffConverter converter;
    
    // First layout
    converter.createFileLayout(scene, params);
    EXPECT_EQ(2, converter.getNumTiffPages());

    // Second layout with different parameters
    configureCommonRanges(params, 3, 1);
    converter.createFileLayout(scene, params);
    EXPECT_EQ(1, converter.getNumTiffPages());
}

TEST(TiffConverterTests, CreateFileLayoutWithChannelNames) {
    class NamedScene : public DummyScene {
    public:
        std::string getChannelName(int channel) const override {
            if (channel == 0) return "Red";
            if (channel == 1) return "Green";
            if (channel == 2) return "Blue";
            return "";
        }
    };

    auto scene = std::make_shared<NamedScene>();
    scene->setNumChannels(3);
    scene->setChannelDataType(DataType::DT_Byte);
    scene->setRect(cv::Rect(0, 0, 512, 512));

    OMETIFFJpegConverterParameters params;
    configureCommonRanges(params, 3, 1);

    TiffConverter converter;
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
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

    TiffConverter converter;
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

    TiffConverter converter;
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

    TiffConverter converter;
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

    TiffConverter converter;
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

    TiffConverter converter;
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

    TiffConverter converter;
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

    TiffConverter converter;
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
    const int tileWidth = 100;
	const int tileHeight = 200;
    params.setTileWidth(tileWidth);
	params.setTileHeight(tileHeight);
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setNumZoomLevels(1);
	Rect rect = params.getRect();
	int tilesX = (rect.width + params.getTileWidth() - 1) / params.getTileWidth();
	int tilesY = (rect.height + params.getTileHeight() - 1) / params.getTileHeight();
	int numTiles = tilesX * tilesY;
    TiffConverter converter;
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
    params.setRect(Rect(10000, 5000, 10000-50, 20000-50));
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
    cv::Rect zoomLevelRect = ConverterTools::computeZoomLevelRect(cv::Rect(0, 0, sceneRect.width, sceneRect.height), cv::Size(tileWidth, tileHeight), 1);
    tilesX = (zoomLevelRect.width + params.getTileWidth() - 1) / params.getTileWidth();
    tilesY = (zoomLevelRect.height + params.getTileHeight() - 1) / params.getTileHeight();
    numTiles += tilesX * tilesY;
	tiffParams->setNumZoomLevels(2);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    totalTiles = converter.getTotalTiles();
    EXPECT_EQ(totalTiles, numTiles);
}

TEST(TiffConverterTests, OMETIFFDefaultSettings) {
    const int numChannels = 4;
    const int numTFrames = 3;
    const int numSlices = 5;
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
    const int numSlices = 3;
    const int numFrames = 5;
    const int numZoomLevels = 5;
	const int numChannels = 3;
	const cv::Rect sceneRect(0, 0, 4096, 8192);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);

    OMETIFFJpegConverterParameters params;

    TiffConverter converter;
	// Full dataset
    params.setChannelRange(cv::Range(0, numChannels));
    params.setSliceRange(cv::Range(0, numSlices));
    params.setTFrameRange(cv::Range(0, numFrames));
    params.setRect(Rect(0, 0, sceneRect.width, sceneRect.height));
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    tiffParams->setTileWidth(256);
    tiffParams->setTileHeight(256);
    tiffParams->setNumZoomLevels(numZoomLevels);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
	int numPages = numSlices * numFrames;
    EXPECT_EQ(15, converter.getNumTiffPages());
	int sliceIndex = 0;
	int frameIndex = 0;
    for (int pageIndex=0; pageIndex<numPages; ++pageIndex) {
		auto page = converter.getTiffPage(pageIndex);
		EXPECT_EQ(cv::Range(0, numChannels), page.getChannelRange());
        EXPECT_EQ(cv::Range(frameIndex,frameIndex+1), page.getTFrameRange());
        EXPECT_EQ(cv::Range(sliceIndex, sliceIndex + 1), page.getZSliceRange());
        sliceIndex++;
        if (sliceIndex >= numSlices) {
            sliceIndex = 0;
            frameIndex++;
		}
		EXPECT_EQ(numZoomLevels-1, page.getNumSubDirectories());
    }
    // Subset of tframes
    int firstFrame = 1;
	int lastFrame = 3;
    int firstSlice = 0;
	int lastSlice = numSlices - 1;
	int numSubsetSlices = lastSlice - firstSlice + 1;
	int numSubsetFrames = lastFrame - firstFrame + 1;
    params.setTFrameRange(cv::Range(firstFrame, lastFrame+1));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numPages = numSubsetSlices * numSubsetFrames;
    EXPECT_EQ(numPages, converter.getNumTiffPages());
    sliceIndex = firstSlice;
    frameIndex = firstFrame;
    for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
        auto page = converter.getTiffPage(pageIndex);
        EXPECT_EQ(cv::Range(0, numChannels), page.getChannelRange());
		int localFrameIndex = frameIndex - firstFrame;
		int localSliceIndex = sliceIndex - firstSlice;
        EXPECT_EQ(cv::Range(localFrameIndex, localFrameIndex + 1), page.getTFrameRange());
        EXPECT_EQ(cv::Range(localSliceIndex, localSliceIndex + 1), page.getZSliceRange());
        EXPECT_EQ(frameIndex, page.getSourceFirstFrame());
		EXPECT_EQ(sliceIndex, page.getSourceFirstSlice());
		EXPECT_EQ(0, page.getSourceFirstChannel());
        sliceIndex++;
        if (sliceIndex > lastSlice) {
            sliceIndex = firstSlice;
            frameIndex++;
        }
        EXPECT_EQ(numZoomLevels - 1, page.getNumSubDirectories());
    }
    // Subset of slices
    firstFrame = 0;
    lastFrame = numFrames - 1;
    firstSlice = 1;
    lastSlice = numSlices - 1;
    numSubsetSlices = lastSlice - firstSlice + 1;
    numSubsetFrames = lastFrame - firstFrame + 1;
    params.setTFrameRange(cv::Range(firstFrame, lastFrame + 1));
	params.setSliceRange(cv::Range(firstSlice, lastSlice + 1));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numPages = numSubsetSlices * numSubsetFrames;
    EXPECT_EQ(numPages, converter.getNumTiffPages());
    sliceIndex = firstSlice;
    frameIndex = firstFrame;
    for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
        auto page = converter.getTiffPage(pageIndex);
        EXPECT_EQ(cv::Range(0, numChannels), page.getChannelRange());
        int localFrameIndex = frameIndex - firstFrame;
        int localSliceIndex = sliceIndex - firstSlice;
        EXPECT_EQ(cv::Range(localFrameIndex, localFrameIndex + 1), page.getTFrameRange());
        EXPECT_EQ(cv::Range(localSliceIndex, localSliceIndex + 1), page.getZSliceRange());
        EXPECT_EQ(frameIndex, page.getSourceFirstFrame());
        EXPECT_EQ(sliceIndex, page.getSourceFirstSlice());
        EXPECT_EQ(0, page.getSourceFirstChannel());
        sliceIndex++;
        if (sliceIndex > lastSlice) {
            sliceIndex = firstSlice;
            frameIndex++;
        }
        EXPECT_EQ(numZoomLevels - 1, page.getNumSubDirectories());
    }
    // Subset of slice & frame subsets
    firstFrame = 2;
    lastFrame = 3;
    firstSlice = 1;
    lastSlice = 2;
    numSubsetSlices = lastSlice - firstSlice + 1;
    numSubsetFrames = lastFrame - firstFrame + 1;
    params.setTFrameRange(cv::Range(firstFrame, lastFrame + 1));
    params.setSliceRange(cv::Range(firstSlice, lastSlice + 1));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numPages = numSubsetSlices * numSubsetFrames;
    EXPECT_EQ(numPages, converter.getNumTiffPages());
    sliceIndex = firstSlice;
    frameIndex = firstFrame;
    for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
        auto page = converter.getTiffPage(pageIndex);
        EXPECT_EQ(cv::Range(0, numChannels), page.getChannelRange());
        int localFrameIndex = frameIndex - firstFrame;
        int localSliceIndex = sliceIndex - firstSlice;
        EXPECT_EQ(cv::Range(localFrameIndex, localFrameIndex + 1), page.getTFrameRange());
        EXPECT_EQ(cv::Range(localSliceIndex, localSliceIndex + 1), page.getZSliceRange());
        EXPECT_EQ(frameIndex, page.getSourceFirstFrame());
        EXPECT_EQ(sliceIndex, page.getSourceFirstSlice());
        EXPECT_EQ(0, page.getSourceFirstChannel());
        sliceIndex++;
        if (sliceIndex > lastSlice) {
            sliceIndex = firstSlice;
            frameIndex++;
        }
        EXPECT_EQ(numZoomLevels - 1, page.getNumSubDirectories());
    }
}

TEST(TiffConverterTests, CreateFileLayoutOMETIFFChannelSubset) {
    const int numSlices = 3;
    const int numFrames = 5;
    const int numZoomLevels = 5;
    const int numChannels = 5;
    const cv::Rect sceneRect(0, 0, 4096, 8192);

    auto scene = makeScene(numChannels);
    scene->setNumTFrames(numFrames);
    scene->setNumZSlices(numSlices);
    scene->setRect(sceneRect);

    OMETIFFJp2KConverterParameters params;

    TiffConverter converter;
    // Subset of slice & frame subsets
    const int firstFrame = 2;
    const int lastFrame = 3;
    const int firstSlice = 1;
    const int lastSlice = 2;
    const int numSubsetSlices = lastSlice - firstSlice + 1;
    const int numSubsetFrames = lastFrame - firstFrame + 1;
    int firstChannel = 0;
	int lastChannel = numChannels - 1;
    params.setTFrameRange(cv::Range(firstFrame, lastFrame + 1));
    params.setSliceRange(cv::Range(firstSlice, lastSlice + 1));
	params.setChannelRange(cv::Range(firstChannel, lastChannel + 1));
	auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
	tiffParams->setNumZoomLevels(numZoomLevels);
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    int numPages = numSubsetSlices * numSubsetFrames * numChannels;
    EXPECT_EQ(numPages, converter.getNumTiffPages());
    int sliceIndex = firstSlice;
    int frameIndex = firstFrame;
	int channelIndex = firstChannel;
    for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
        auto page = converter.getTiffPage(pageIndex);
        int localFrameIndex = frameIndex - firstFrame;
        int localSliceIndex = sliceIndex - firstSlice;
		int localChannelIndex = channelIndex - firstChannel;
        EXPECT_EQ(cv::Range(localFrameIndex, localFrameIndex + 1), page.getTFrameRange());
        EXPECT_EQ(cv::Range(localSliceIndex, localSliceIndex + 1), page.getZSliceRange());
		EXPECT_EQ(cv::Range(localChannelIndex, localChannelIndex + 1), page.getChannelRange());
        EXPECT_EQ(frameIndex, page.getSourceFirstFrame());
        EXPECT_EQ(sliceIndex, page.getSourceFirstSlice());
        EXPECT_EQ(channelIndex, page.getSourceFirstChannel());
        channelIndex++;
        if (channelIndex > lastChannel) {
			channelIndex = firstChannel;
            sliceIndex++;
            if (sliceIndex > lastSlice) {
				channelIndex = firstChannel;
                sliceIndex = firstSlice;
                frameIndex++;
            }
        }
        EXPECT_EQ(numZoomLevels - 1, page.getNumSubDirectories());
    }

    firstChannel = 2;
    lastChannel = 3;
	int numSubsetChannels = lastChannel - firstChannel + 1;
    params.setTFrameRange(cv::Range(firstFrame, lastFrame + 1));
    params.setSliceRange(cv::Range(firstSlice, lastSlice + 1));
    params.setChannelRange(cv::Range(firstChannel, lastChannel + 1));
    ASSERT_NO_THROW(converter.createFileLayout(scene, params));
    numPages = numSubsetSlices * numSubsetFrames * numSubsetChannels;
    EXPECT_EQ(numPages, converter.getNumTiffPages());
    sliceIndex = firstSlice;
    frameIndex = firstFrame;
    channelIndex = firstChannel;
    for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
        auto page = converter.getTiffPage(pageIndex);
        int localFrameIndex = frameIndex - firstFrame;
        int localSliceIndex = sliceIndex - firstSlice;
        int localChannelIndex = channelIndex - firstChannel;
        EXPECT_EQ(cv::Range(localFrameIndex, localFrameIndex + 1), page.getTFrameRange());
        EXPECT_EQ(cv::Range(localSliceIndex, localSliceIndex + 1), page.getZSliceRange());
        EXPECT_EQ(cv::Range(localChannelIndex, localChannelIndex + 1), page.getChannelRange());
        EXPECT_EQ(frameIndex, page.getSourceFirstFrame());
        EXPECT_EQ(sliceIndex, page.getSourceFirstSlice());
        EXPECT_EQ(channelIndex, page.getSourceFirstChannel());
        channelIndex++;
        if (channelIndex > lastChannel) {
            channelIndex = firstChannel;
            sliceIndex++;
            if (sliceIndex > lastSlice) {
                channelIndex = firstChannel;
                sliceIndex = firstSlice;
                frameIndex++;
            }
        }
        EXPECT_EQ(numZoomLevels - 1, page.getNumSubDirectories());
    }

}


TEST(TiffConverterTests, SVSDefaultSettings) {
    const int numChannels = 3;
    const int numTFrames = 1;
    const int numSlices = 1;
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

TEST(TiffConverterTests, jpegToOMETIFF)
{
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
        [&progress, &progressLast](int pr) { progressLast = pr; progress.insert(pr); }));
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

TEST(TiffConverterTests, jpegToSVS)
{
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
    ASSERT_NO_THROW(converter.createTiff(outputPath, nullptr));
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
