#include <gtest/gtest.h>
#include "slideio/converter/converterparameters.hpp"
#include "slideio/base/exceptions.hpp"
#include "tests/testlib/testscene.hpp"

using namespace slideio;
using namespace slideio::converter;

TEST(ConverterParametersTests, DefaultConstructor) {
    ConverterParameters params;
    EXPECT_EQ(ImageFormat::Unknown, params.getFormat());
    EXPECT_FALSE(params.isValid());
}

TEST(ConverterParametersTests, ValidParametersAfterConstruction) {
    ConverterParameters params(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(ImageFormat::SVS, params.getFormat());
    EXPECT_EQ(Container::TIFF_CONTAINER, params.getContainerType());
    EXPECT_EQ(Compression::Jpeg, params.getEncoding());
}

TEST(ConverterParametersTests, SetAndGetRect) {
    ConverterParameters params(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    Rect rect(10, 20, 100, 200);
    params.setRect(rect);
    const Rect& result = params.getRect();
    EXPECT_EQ(10, result.x);
    EXPECT_EQ(20, result.y);
    EXPECT_EQ(100, result.width);
    EXPECT_EQ(200, result.height);
}

TEST(ConverterParametersTests, SetAndGetSliceRange) {
    ConverterParameters params(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, Compression::Jpeg2000);
    cv::Range range(0, 5);
    params.setSliceRange(range);
    EXPECT_EQ(cv::Range(0, 5), params.getSliceRange());
}

TEST(ConverterParametersTests, SetAndGetChannelRange) {
    ConverterParameters params(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, Compression::Jpeg);
    cv::Range range(1, 4);
    params.setChannelRange(range);
    EXPECT_EQ(cv::Range(1, 4), params.getChannelRange());
}

TEST(ConverterParametersTests, SetAndGetTFrameRange) {
    ConverterParameters params(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, Compression::Jpeg2000);
    cv::Range range(2, 8);
    params.setTFrameRange(range);
    EXPECT_EQ(cv::Range(2, 8), params.getTFrameRange());
}

TEST(ConverterParametersTests, GetEncodeParameters) {
    ConverterParameters params(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    auto encodeParams = params.getEncodeParameters();
    ASSERT_NE(nullptr, encodeParams);
    EXPECT_EQ(Compression::Jpeg, encodeParams->getCompression());
}

TEST(ConverterParametersTests, GetContainerParameters) {
    ConverterParameters params(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    auto containerParams = params.getContainerParameters();
    ASSERT_NE(nullptr, containerParams);
    EXPECT_EQ(Container::TIFF_CONTAINER, containerParams->getContainerType());
}

TEST(ConverterParametersTests, SVSJpegConverterParameters) {
    SVSJpegConverterParameters params;
    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(ImageFormat::SVS, params.getFormat());
    EXPECT_EQ(Compression::Jpeg, params.getEncoding());
    EXPECT_EQ(Container::TIFF_CONTAINER, params.getContainerType());
    
    params.setTileWidth(512);
    params.setTileHeight(512);
    EXPECT_EQ(512, params.getTileWidth());
    EXPECT_EQ(512, params.getTileHeight());
    
    params.setQuality(85);
    auto encodeParams = std::static_pointer_cast<JpegEncodeParameters>(params.getEncodeParameters());
    EXPECT_EQ(85, encodeParams->getQuality());
}

TEST(ConverterParametersTests, SVSJp2KConverterParameters) {
    SVSJp2KConverterParameters params;
    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(ImageFormat::SVS, params.getFormat());
    EXPECT_EQ(Compression::Jpeg2000, params.getEncoding());
    
    params.setTileWidth(256);
    params.setTileHeight(256);
    EXPECT_EQ(256, params.getTileWidth());
    EXPECT_EQ(256, params.getTileHeight());
    
    params.setCompressionRate(0.5f);
    auto encodeParams = std::static_pointer_cast<JP2KEncodeParameters>(params.getEncodeParameters());
    EXPECT_FLOAT_EQ(0.5f, encodeParams->getCompressionRate());
}

TEST(ConverterParametersTests, OMETIFFJpegConverterParameters) {
    OMETIFFJpegConverterParameters params;
    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(ImageFormat::OME_TIFF, params.getFormat());
    EXPECT_EQ(Compression::Jpeg, params.getEncoding());
    EXPECT_EQ(Container::TIFF_CONTAINER, params.getContainerType());
    
    params.setTileWidth(1024);
    params.setTileHeight(1024);
    EXPECT_EQ(1024, params.getTileWidth());
    EXPECT_EQ(1024, params.getTileHeight());
    
    params.setQuality(90);
    auto encodeParams = std::static_pointer_cast<JpegEncodeParameters>(params.getEncodeParameters());
    EXPECT_EQ(90, encodeParams->getQuality());
}

TEST(ConverterParametersTests, OMETIFFJp2KConverterParameters) {
    OMETIFFJp2KConverterParameters params;
    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(ImageFormat::OME_TIFF, params.getFormat());
    EXPECT_EQ(Compression::Jpeg2000, params.getEncoding());
    
    params.setTileWidth(128);
    params.setTileHeight(128);
    EXPECT_EQ(128, params.getTileWidth());
    EXPECT_EQ(128, params.getTileHeight());
    
    params.setCompressionRate(0.75f);
    auto encodeParams = std::static_pointer_cast<JP2KEncodeParameters>(params.getEncodeParameters());
    EXPECT_FLOAT_EQ(0.75f, encodeParams->getCompressionRate());
}

TEST(ConverterParametersTests, TIFFContainerParametersDefaults) {
    SVSJpegConverterParameters params;
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(256, containerParams->getTileWidth());
    EXPECT_EQ(256, containerParams->getTileHeight());
    EXPECT_EQ(-1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, TIFFContainerParametersSetters) {
    OMETIFFJpegConverterParameters params;
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    
    containerParams->setTileWidth(512);
    containerParams->setTileHeight(256);
    containerParams->setNumZoomLevels(4);
    
    EXPECT_EQ(512, containerParams->getTileWidth());
    EXPECT_EQ(256, containerParams->getTileHeight());
    EXPECT_EQ(4, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UnsupportedCompressionThrows) {
    EXPECT_THROW(
        ConverterParameters params(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Uncompressed),
        RuntimeError
    );
}

TEST(ConverterParametersTests, UnsupportedContainerThrows) {
    EXPECT_THROW(
        ConverterParameters params(ImageFormat::SVS, Container::UNKNOWN_CONTAINER, Compression::Jpeg),
        RuntimeError
    );
}

namespace {
    class MockScene : public TestScene {
    public:
        void readResampledBlockChannelsEx(const cv::Rect&, const cv::Size&, const std::vector<int>&,
            int, int, cv::OutputArray) override {}
        
        int getNumZSlices() const override { return m_numZSlices; }
        void setNumZSlices(int numZSlices) { m_numZSlices = numZSlices; }
        
        int getNumTFrames() const override { return m_numTFrames; }
        void setNumTFrames(int numTFrames) { m_numTFrames = numTFrames; }
        
    private:
        int m_numZSlices = 1;
        int m_numTFrames = 1;
    };
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_SVS_AllUndefined) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(100, 200, 1024, 2048));
    scene->setNumChannels(3);
    scene->setNumZSlices(5);
    scene->setNumTFrames(10);
    
    params.updateNotDefinedParameters(scene);
    
    // Rect should be set to scene rect
    const Rect& rect = params.getRect();
    EXPECT_EQ(100, rect.x);
    EXPECT_EQ(200, rect.y);
    EXPECT_EQ(1024, rect.width);
    EXPECT_EQ(2048, rect.height);
    
    // For SVS format, channel range should include all channels
    EXPECT_EQ(cv::Range(0, 3), params.getChannelRange());
    
    // For SVS format, slice range should be (0, 1) regardless of scene slices
    EXPECT_EQ(cv::Range(0, 1), params.getSliceRange());
    
    // For SVS format, frame range should be (0, 1) regardless of scene frames
    EXPECT_EQ(cv::Range(0, 1), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_OMETIFF_AllUndefined) {
    OMETIFFJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 768));
    scene->setNumChannels(4);
    scene->setNumZSlices(7);
    scene->setNumTFrames(12);
    
    params.updateNotDefinedParameters(scene);
    
    // Rect should be set to scene rect
    const Rect& rect = params.getRect();
    EXPECT_EQ(0, rect.x);
    EXPECT_EQ(0, rect.y);
    EXPECT_EQ(512, rect.width);
    EXPECT_EQ(768, rect.height);
    
    // For OME-TIFF format, channel range should include all channels
    EXPECT_EQ(cv::Range(0, 4), params.getChannelRange());
    
    // For OME-TIFF format, slice range should include all slices from scene
    EXPECT_EQ(cv::Range(0, 7), params.getSliceRange());
    
    // For OME-TIFF format, frame range should include all frames from scene
    EXPECT_EQ(cv::Range(0, 12), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_PreserveDefinedRect) {
    SVSJpegConverterParameters params;
    
    // Set a specific rect before update
    Rect customRect(50, 100, 256, 512);
    params.setRect(customRect);
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1024, 2048));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Custom rect should be preserved
    const Rect& rect = params.getRect();
    EXPECT_EQ(50, rect.x);
    EXPECT_EQ(100, rect.y);
    EXPECT_EQ(256, rect.width);
    EXPECT_EQ(512, rect.height);
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_PreserveDefinedChannelRange) {
    OMETIFFJp2KConverterParameters params;
    
    // Set a specific channel range before update
    params.setChannelRange(cv::Range(1, 3));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(5);
    scene->setNumZSlices(3);
    scene->setNumTFrames(4);
    
    params.updateNotDefinedParameters(scene);
    
    // Custom channel range should be preserved
    EXPECT_EQ(cv::Range(1, 3), params.getChannelRange());
    
    // Other undefined parameters should be set
    EXPECT_EQ(cv::Range(0, 3), params.getSliceRange());
    EXPECT_EQ(cv::Range(0, 4), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_PreserveDefinedSliceRange) {
    OMETIFFJpegConverterParameters params;
    
    // Set a specific slice range before update
    params.setSliceRange(cv::Range(2, 5));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(3);
    scene->setNumZSlices(10);
    scene->setNumTFrames(8);
    
    params.updateNotDefinedParameters(scene);
    
    // Custom slice range should be preserved
    EXPECT_EQ(cv::Range(2, 5), params.getSliceRange());
    
    // Other undefined parameters should be set
    EXPECT_EQ(cv::Range(0, 3), params.getChannelRange());
    EXPECT_EQ(cv::Range(0, 8), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_PreserveDefinedFrameRange) {
    OMETIFFJp2KConverterParameters params;
    
    // Set a specific frame range before update
    params.setTFrameRange(cv::Range(3, 7));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(3);
    scene->setNumZSlices(5);
    scene->setNumTFrames(10);
    
    params.updateNotDefinedParameters(scene);
    
    // Custom frame range should be preserved
    EXPECT_EQ(cv::Range(3, 7), params.getTFrameRange());
    
    // Other undefined parameters should be set
    EXPECT_EQ(cv::Range(0, 3), params.getChannelRange());
    EXPECT_EQ(cv::Range(0, 5), params.getSliceRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_InvalidRect) {
    SVSJp2KConverterParameters params;
    
    // Set an invalid rect (default constructor creates invalid rect)
    params.setRect(Rect(0, 0, 0, 0));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(10, 20, 800, 600));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Invalid rect should be replaced with scene rect
    const Rect& rect = params.getRect();
    EXPECT_EQ(10, rect.x);
    EXPECT_EQ(20, rect.y);
    EXPECT_EQ(800, rect.width);
    EXPECT_EQ(600, rect.height);
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_EmptyChannelRange) {
    OMETIFFJpegConverterParameters params;
    
    // Empty channel range (size = 0)
    params.setChannelRange(cv::Range(0, 0));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(4);
    scene->setNumZSlices(2);
    scene->setNumTFrames(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Empty channel range should be replaced
    EXPECT_EQ(cv::Range(0, 4), params.getChannelRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_EmptySliceRange) {
    OMETIFFJp2KConverterParameters params;
    
    // Empty slice range
    params.setSliceRange(cv::Range(0, 0));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(3);
    scene->setNumZSlices(6);
    scene->setNumTFrames(4);
    
    params.updateNotDefinedParameters(scene);
    
    // Empty slice range should be replaced
    EXPECT_EQ(cv::Range(0, 6), params.getSliceRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_EmptyFrameRange) {
    OMETIFFJpegConverterParameters params;
    
    // Empty frame range
    params.setTFrameRange(cv::Range(0, 0));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(3);
    scene->setNumZSlices(2);
    scene->setNumTFrames(8);
    
    params.updateNotDefinedParameters(scene);
    
    // Empty frame range should be replaced
    EXPECT_EQ(cv::Range(0, 8), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_SVS_ForcesSliceAndFrameToOne) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1024, 1024));
    scene->setNumChannels(3);
    scene->setNumZSlices(10);  // Scene has multiple slices
    scene->setNumTFrames(15);  // Scene has multiple frames
    
    params.updateNotDefinedParameters(scene);
    
    // SVS format should force slice and frame ranges to (0, 1)
    EXPECT_EQ(cv::Range(0, 1), params.getSliceRange());
    EXPECT_EQ(cv::Range(0, 1), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_MixedDefinedUndefined) {
    OMETIFFJp2KConverterParameters params;
    
    // Define some parameters but not all
    params.setRect(Rect(100, 200, 512, 768));
    params.setChannelRange(cv::Range(0, 2));
    // Slice and frame ranges are undefined
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 2048, 2048));
    scene->setNumChannels(5);
    scene->setNumZSlices(4);
    scene->setNumTFrames(6);
    
    params.updateNotDefinedParameters(scene);
    
    // Defined parameters should be preserved
    const Rect& rect = params.getRect();
    EXPECT_EQ(100, rect.x);
    EXPECT_EQ(200, rect.y);
    EXPECT_EQ(512, rect.width);
    EXPECT_EQ(768, rect.height);
    EXPECT_EQ(cv::Range(0, 2), params.getChannelRange());
    
    // Undefined parameters should be set from scene
    EXPECT_EQ(cv::Range(0, 4), params.getSliceRange());
    EXPECT_EQ(cv::Range(0, 6), params.getTFrameRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_SceneWithZeroChannels) {
    OMETIFFJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 512, 512));
    scene->setNumChannels(0);  // Edge case: no channels
    scene->setNumZSlices(1);
    scene->setNumTFrames(1);
    
    params.updateNotDefinedParameters(scene);
    
    // Channel range should handle zero channels
    EXPECT_EQ(cv::Range(0, 0), params.getChannelRange());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_SceneWithSingleSliceAndFrame) {
    SVSJp2KConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1024, 1024));
    scene->setNumChannels(3);
    scene->setNumZSlices(1);   // Single slice
    scene->setNumTFrames(1);   // Single frame
    
    params.updateNotDefinedParameters(scene);
    
    // Should work correctly with single slice/frame
    EXPECT_EQ(cv::Range(0, 1), params.getSliceRange());
    EXPECT_EQ(cv::Range(0, 1), params.getTFrameRange());
}

TEST(ConverterParametersTests, CopyConstructor_BasicProperties) {
    ConverterParameters original(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    original.setRect(Rect(10, 20, 100, 200));
    original.setChannelRange(cv::Range(0, 3));
    original.setSliceRange(cv::Range(0, 1));
    original.setTFrameRange(cv::Range(0, 1));
    
    ConverterParameters copy(original);
    
    // Verify basic properties are copied
    EXPECT_EQ(ImageFormat::SVS, copy.getFormat());
    EXPECT_EQ(Compression::Jpeg, copy.getEncoding());
    EXPECT_EQ(Container::TIFF_CONTAINER, copy.getContainerType());
    
    const Rect& rect = copy.getRect();
    EXPECT_EQ(10, rect.x);
    EXPECT_EQ(20, rect.y);
    EXPECT_EQ(100, rect.width);
    EXPECT_EQ(200, rect.height);
    
    EXPECT_EQ(cv::Range(0, 3), copy.getChannelRange());
    EXPECT_EQ(cv::Range(0, 1), copy.getSliceRange());
    EXPECT_EQ(cv::Range(0, 1), copy.getTFrameRange());
}

TEST(ConverterParametersTests, CopyConstructor_JpegParameters) {
    ConverterParameters original(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    auto jpegParams = std::static_pointer_cast<JpegEncodeParameters>(original.getEncodeParameters());
    jpegParams->setQuality(85);
    
    ConverterParameters copy(original);
    
    // Verify encode parameters are deep copied
    auto copiedParams = std::static_pointer_cast<JpegEncodeParameters>(copy.getEncodeParameters());
    ASSERT_NE(nullptr, copiedParams);
    EXPECT_EQ(85, copiedParams->getQuality());
    
    // Verify it's a deep copy - changing original shouldn't affect copy
    jpegParams->setQuality(75);
    EXPECT_EQ(85, copiedParams->getQuality());
}

TEST(ConverterParametersTests, CopyConstructor_JP2KParameters) {
    ConverterParameters original(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, Compression::Jpeg2000);
    auto jp2kParams = std::static_pointer_cast<JP2KEncodeParameters>(original.getEncodeParameters());
    jp2kParams->setCompressionRate(0.5f);
    jp2kParams->setSubSamplingDx(2);
    jp2kParams->setSubSamplingDy(2);
    jp2kParams->setCodecFormat(JP2KEncodeParameters::Codec::J2KFile);
    
    ConverterParameters copy(original);
    
    // Verify encode parameters are deep copied
    auto copiedParams = std::static_pointer_cast<JP2KEncodeParameters>(copy.getEncodeParameters());
    ASSERT_NE(nullptr, copiedParams);
    EXPECT_FLOAT_EQ(0.5f, copiedParams->getCompressionRate());
    EXPECT_EQ(2, copiedParams->getSubSamplingDx());
    EXPECT_EQ(2, copiedParams->getSubSamplingDy());
    EXPECT_EQ(JP2KEncodeParameters::Codec::J2KFile, copiedParams->getCodecFormat());
    
    // Verify it's a deep copy
    jp2kParams->setCompressionRate(0.75f);
    EXPECT_FLOAT_EQ(0.5f, copiedParams->getCompressionRate());
}

TEST(ConverterParametersTests, CopyConstructor_TIFFContainerParameters) {
    ConverterParameters original(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(original.getContainerParameters());
    tiffParams->setTileWidth(512);
    tiffParams->setTileHeight(512);
    tiffParams->setNumZoomLevels(5);
    
    ConverterParameters copy(original);
    
    // Verify container parameters are deep copied
    auto copiedParams = std::static_pointer_cast<TIFFContainerParameters>(copy.getContainerParameters());
    ASSERT_NE(nullptr, copiedParams);
    EXPECT_EQ(512, copiedParams->getTileWidth());
    EXPECT_EQ(512, copiedParams->getTileHeight());
    EXPECT_EQ(5, copiedParams->getNumZoomLevels());
    
    // Verify it's a deep copy
    tiffParams->setTileWidth(1024);
    EXPECT_EQ(512, copiedParams->getTileWidth());
}

TEST(ConverterParametersTests, AssignmentOperator_BasicProperties) {
    ConverterParameters original(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, Compression::Jpeg2000);
    original.setRect(Rect(50, 100, 300, 400));
    original.setChannelRange(cv::Range(1, 4));
    original.setSliceRange(cv::Range(0, 5));
    original.setTFrameRange(cv::Range(0, 10));
    
    ConverterParameters copy;
    copy = original;
    
    // Verify basic properties are copied
    EXPECT_EQ(ImageFormat::OME_TIFF, copy.getFormat());
    EXPECT_EQ(Compression::Jpeg2000, copy.getEncoding());
    EXPECT_EQ(Container::TIFF_CONTAINER, copy.getContainerType());
    
    const Rect& rect = copy.getRect();
    EXPECT_EQ(50, rect.x);
    EXPECT_EQ(100, rect.y);
    EXPECT_EQ(300, rect.width);
    EXPECT_EQ(400, rect.height);
    
    EXPECT_EQ(cv::Range(1, 4), copy.getChannelRange());
    EXPECT_EQ(cv::Range(0, 5), copy.getSliceRange());
    EXPECT_EQ(cv::Range(0, 10), copy.getTFrameRange());
}

TEST(ConverterParametersTests, AssignmentOperator_JpegParameters) {
    ConverterParameters original(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    auto jpegParams = std::static_pointer_cast<JpegEncodeParameters>(original.getEncodeParameters());
    jpegParams->setQuality(90);
    
    ConverterParameters copy;
    copy = original;
    
    // Verify encode parameters are deep copied
    auto copiedParams = std::static_pointer_cast<JpegEncodeParameters>(copy.getEncodeParameters());
    ASSERT_NE(nullptr, copiedParams);
    EXPECT_EQ(90, copiedParams->getQuality());
    
    // Verify it's a deep copy
    jpegParams->setQuality(80);
    EXPECT_EQ(90, copiedParams->getQuality());
}

TEST(ConverterParametersTests, AssignmentOperator_JP2KParameters) {
    ConverterParameters original(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, Compression::Jpeg2000);
    auto jp2kParams = std::static_pointer_cast<JP2KEncodeParameters>(original.getEncodeParameters());
    jp2kParams->setCompressionRate(0.3f);
    jp2kParams->setSubSamplingDx(4);
    jp2kParams->setSubSamplingDy(4);
    
    ConverterParameters copy;
    copy = original;
    
    // Verify encode parameters are deep copied
    auto copiedParams = std::static_pointer_cast<JP2KEncodeParameters>(copy.getEncodeParameters());
    ASSERT_NE(nullptr, copiedParams);
    EXPECT_FLOAT_EQ(0.3f, copiedParams->getCompressionRate());
    EXPECT_EQ(4, copiedParams->getSubSamplingDx());
    EXPECT_EQ(4, copiedParams->getSubSamplingDy());
    
    // Verify it's a deep copy
    jp2kParams->setCompressionRate(0.6f);
    EXPECT_FLOAT_EQ(0.3f, copiedParams->getCompressionRate());
}

TEST(ConverterParametersTests, AssignmentOperator_TIFFContainerParameters) {
    ConverterParameters original(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(original.getContainerParameters());
    tiffParams->setTileWidth(1024);
    tiffParams->setTileHeight(1024);
    tiffParams->setNumZoomLevels(3);
    
    ConverterParameters copy;
    copy = original;
    
    // Verify container parameters are deep copied
    auto copiedParams = std::static_pointer_cast<TIFFContainerParameters>(copy.getContainerParameters());
    ASSERT_NE(nullptr, copiedParams);
    EXPECT_EQ(1024, copiedParams->getTileWidth());
    EXPECT_EQ(1024, copiedParams->getTileHeight());
    EXPECT_EQ(3, copiedParams->getNumZoomLevels());
    
    // Verify it's a deep copy
    tiffParams->setTileWidth(128);
    EXPECT_EQ(1024, copiedParams->getTileWidth());
}

TEST(ConverterParametersTests, AssignmentOperator_SelfAssignment) {
    ConverterParameters params(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    params.setRect(Rect(10, 20, 100, 200));
    
    params = params;
    
    // Verify self-assignment doesn't break anything
    EXPECT_EQ(ImageFormat::SVS, params.getFormat());
    const Rect& rect = params.getRect();
    EXPECT_EQ(10, rect.x);
    EXPECT_EQ(20, rect.y);
    EXPECT_EQ(100, rect.width);
    EXPECT_EQ(200, rect.height);
}

TEST(ConverterParametersTests, CopyConstructor_DefaultParameters) {
    ConverterParameters original;
    
    ConverterParameters copy(original);
    
    // Verify default state is copied
    EXPECT_EQ(ImageFormat::Unknown, copy.getFormat());
    EXPECT_FALSE(copy.isValid());
}

TEST(ConverterParametersTests, CopyConstructor_SVSJpegDerivedClass) {
    SVSJpegConverterParameters original;
    original.setTileWidth(512);
    original.setTileHeight(512);
    original.setQuality(85);
    original.setRect(Rect(0, 0, 1024, 1024));
    
    SVSJpegConverterParameters copy(original);
    
    // Verify all properties are copied
    EXPECT_EQ(ImageFormat::SVS, copy.getFormat());
    EXPECT_EQ(512, copy.getTileWidth());
    EXPECT_EQ(512, copy.getTileHeight());
    
    auto jpegParams = std::static_pointer_cast<JpegEncodeParameters>(copy.getEncodeParameters());
    EXPECT_EQ(85, jpegParams->getQuality());
    
    const Rect& rect = copy.getRect();
    EXPECT_EQ(0, rect.x);
    EXPECT_EQ(0, rect.y);
    EXPECT_EQ(1024, rect.width);
    EXPECT_EQ(1024, rect.height);
}

TEST(ConverterParametersTests, AssignmentOperator_OMETIFFJp2KDerivedClass) {
    OMETIFFJp2KConverterParameters original;
    original.setTileWidth(256);
    original.setTileHeight(256);
    original.setCompressionRate(0.5f);
    original.setRect(Rect(100, 100, 512, 512));
    original.setChannelRange(cv::Range(0, 4));
    
    OMETIFFJp2KConverterParameters copy;
    copy = original;
    
    // Verify all properties are copied
    EXPECT_EQ(ImageFormat::OME_TIFF, copy.getFormat());
    EXPECT_EQ(256, copy.getTileWidth());
    EXPECT_EQ(256, copy.getTileHeight());
    
    auto jp2kParams = std::static_pointer_cast<JP2KEncodeParameters>(copy.getEncodeParameters());
    EXPECT_FLOAT_EQ(0.5f, jp2kParams->getCompressionRate());
    
    const Rect& rect = copy.getRect();
    EXPECT_EQ(100, rect.x);
    EXPECT_EQ(100, rect.y);
    EXPECT_EQ(512, rect.width);
    EXPECT_EQ(512, rect.height);
    
    EXPECT_EQ(cv::Range(0, 4), copy.getChannelRange());
}

TEST(ConverterParametersTests, CopyConstructor_IndependentModification) {
    ConverterParameters original(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    original.setRect(Rect(10, 10, 100, 100));
    original.setChannelRange(cv::Range(0, 3));
    
    ConverterParameters copy(original);
    
    // Modify the copy
    copy.setRect(Rect(20, 20, 200, 200));
    copy.setChannelRange(cv::Range(1, 4));
    
    // Verify original is unchanged
    const Rect& originalRect = original.getRect();
    EXPECT_EQ(10, originalRect.x);
    EXPECT_EQ(10, originalRect.y);
    EXPECT_EQ(100, originalRect.width);
    EXPECT_EQ(100, originalRect.height);
    EXPECT_EQ(cv::Range(0, 3), original.getChannelRange());
    
    // Verify copy has new values
    const Rect& copyRect = copy.getRect();
    EXPECT_EQ(20, copyRect.x);
    EXPECT_EQ(20, copyRect.y);
    EXPECT_EQ(200, copyRect.width);
    EXPECT_EQ(200, copyRect.height);
    EXPECT_EQ(cv::Range(1, 4), copy.getChannelRange());
}

TEST(ConverterParametersTests, AssignmentOperator_ChainedAssignment) {
    ConverterParameters params1(ImageFormat::SVS, Container::TIFF_CONTAINER, Compression::Jpeg);
    params1.setRect(Rect(5, 5, 50, 50));
    
    ConverterParameters params2, params3;
    
    // Test chained assignment
    params3 = params2 = params1;
    
    // Verify all three have the same values
    EXPECT_EQ(ImageFormat::SVS, params2.getFormat());
    EXPECT_EQ(ImageFormat::SVS, params3.getFormat());
    
    const Rect& rect2 = params2.getRect();
    const Rect& rect3 = params3.getRect();
    
    EXPECT_EQ(5, rect2.x);
    EXPECT_EQ(5, rect2.y);
    EXPECT_EQ(50, rect2.width);
    EXPECT_EQ(50, rect2.height);
    
    EXPECT_EQ(5, rect3.x);
    EXPECT_EQ(5, rect3.y);
    EXPECT_EQ(50, rect3.width);
    EXPECT_EQ(50, rect3.height);
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_SmallImage) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 500, 600));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Small image (both dimensions <= 1000) should have 1 zoom level
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_MediumImage) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1500, 1200));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Medium image (one dimension > 1000) should have 2 zoom levels
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(2, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_LargeImage) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 16000, 16000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Large image (16000x16000) should have 5 zoom levels
    // 16000 -> 8000 -> 4000 -> 2000 -> 1000 -> 500 (stops at 1000)
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(5, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_ExactlyAtThreshold) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1000, 1000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Exactly 1000x1000 should have 1 zoom level (not > 1000)
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_JustOverThreshold) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1001, 1001));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Just over 1000x1000 (1001x1001) should have 2 zoom levels
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(2, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_AsymmetricImage) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 100000, 500));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Asymmetric image where only width is large
    // Both dimensions must be > 1000 for additional zoom levels
    // 100000 > 1000, but 500 <= 1000, so only 1 zoom level
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_PreserveIfDefined) {
    SVSJpegConverterParameters params;
    
    // Pre-set zoom levels
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    containerParams->setNumZoomLevels(10);
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 16000, 16000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Pre-defined zoom levels (>= 1) should be preserved
    EXPECT_EQ(10, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_UpdateIfNegative) {
    SVSJpegConverterParameters params;
    
    // Set zoom levels to default value (-1)
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(-1, containerParams->getNumZoomLevels());
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 5000, 5000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Negative zoom levels should be updated
    // 5000 -> 2500 -> 1250 -> 625 (stops at 1000)
    EXPECT_EQ(4, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_UpdateIfZero) {
    SVSJpegConverterParameters params;
    
    // Set zoom levels to 0 (invalid)
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    containerParams->setNumZoomLevels(0);
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 4000, 4000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Zero zoom levels should be updated
    // 4000 -> 2000 -> 1000 -> 500 (stops at 1000)
    EXPECT_EQ(3, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_WithCustomRect) {
    SVSJpegConverterParameters params;
    
    // Set a custom rect smaller than the scene
    params.setRect(Rect(100, 100, 2500, 2500));
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 10000, 10000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Zoom levels should be computed based on the custom rect (2500x2500)
    // 2500 -> 1250 -> 625 (stops at 1000)
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(3, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_VeryLargeImage) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 64000, 64000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Very large image (64000x64000)
    // 64000 -> 32000 -> 16000 -> 8000 -> 4000 -> 2000 -> 1000 -> 500
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(7, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_OMETIFFFormat) {
    OMETIFFJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 8000, 8000));
    scene->setNumChannels(3);
    scene->setNumZSlices(5);
    scene->setNumTFrames(10);
    
    params.updateNotDefinedParameters(scene);
    
    // OME-TIFF should also compute zoom levels
    // 8000 -> 4000 -> 2000 -> 1000 -> 500
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(4, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_MinimalRect) {
    SVSJp2KConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 1, 1));
    scene->setNumChannels(1);
    
    params.updateNotDefinedParameters(scene);
    
    // Minimal rect should have 1 zoom level
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_TallNarrowImage) {
    SVSJpegConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 800, 50000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Tall narrow image where height > 1000 but width <= 1000
    // Both must be > 1000 for additional zoom levels
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_WideShortImage) {
    SVSJp2KConverterParameters params;
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 50000, 800));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Wide short image where width > 1000 but height <= 1000
    // Both must be > 1000 for additional zoom levels
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    EXPECT_EQ(1, containerParams->getNumZoomLevels());
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_NullContainerParameters) {
    ConverterParameters params;
    
    // Default constructor leaves container parameters null
    EXPECT_FALSE(params.isValid());
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 5000, 5000));
    scene->setNumChannels(3);
    
    // Should not crash with null container parameters
    EXPECT_NO_THROW(params.updateNotDefinedParameters(scene));
}

TEST(ConverterParametersTests, UpdateNotDefinedParameters_ZoomLevels_CompleteWorkflow) {
    SVSJpegConverterParameters params;
    params.setQuality(95);
    
    auto scene = std::make_shared<MockScene>();
    scene->setRect(cv::Rect(0, 0, 15000, 12000));
    scene->setNumChannels(3);
    
    params.updateNotDefinedParameters(scene);
    
    // Verify all parameters are set correctly
    auto containerParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
    
    // Check that zoom levels were computed
    EXPECT_GT(containerParams->getNumZoomLevels(), 1);
    
    // Check that rect was set
    const Rect& rect = params.getRect();
    EXPECT_EQ(15000, rect.width);
    EXPECT_EQ(12000, rect.height);
    
    // Check that channel range was set
    EXPECT_EQ(cv::Range(0, 3), params.getChannelRange());
    
    // Check that quality was preserved
    auto jpegParams = std::static_pointer_cast<JpegEncodeParameters>(params.getEncodeParameters());
    EXPECT_EQ(95, jpegParams->getQuality());
}
