// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scenesource.hpp"
#include "slideio/slideio/scenemetadata.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/converter/convertertools.hpp"
#include "slideio/core/cvscene.hpp"
#include "tests/testlib/testscenesource.hpp"
#include "tests/testlib/testscene.hpp"
#include "tests/testlib/testtools.hpp"
#include <filesystem>
#include <vector>

using namespace slideio;

namespace
{
    SceneMetadata computedSceneMetadata()
    {
        SceneMetadata m;
        m.name = "computed";
        m.rect = Rect(0, 0, 512, 512);
        m.channelDataTypes.assign(3, DataType::DT_Byte);
        m.resolution = Resolution(2.5e-7, 2.5e-7);
        m.magnification = 20.0;
        m.levels.emplace_back(0, Size(512, 512), 1.0, 20.0, Size(256, 256));
        return m;
    }

    std::string tempOutputPath(const char* name)
    {
        const std::string path = (std::filesystem::temp_directory_path() / name).string();
        std::filesystem::remove(path);
        return path;
    }

    // A converter whose per-thread scene clone fails, as re-opening a scene by
    // (file path, driver id) does for a scene no driver can open.
    class FailingCloneConverter : public converter::TiffConverter
    {
    public:
        std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> cloneScene() const override
        {
            RAISE_RUNTIME_ERROR << "FailingCloneConverter: deliberate clone failure";
        }
    };
}

TEST(SourceSceneConversion, aComputedSceneConvertsToOmeTiffAndReadsBack)
{
    auto source = std::make_shared<TestSceneSource>();
    auto scene = makeScene(source, computedSceneMetadata());
    const std::string outPath = tempOutputPath("slideio_source_scene.ome.tiff");

    // ConverterParameters accepts only Jpeg and Jpeg2000, so Uncompressed is
    // not available. A JPEG 2000 compression rate of 1 (or less) makes
    // OpenJPEG encode losslessly (reversible wavelet, rate 0), so every pixel
    // can be checked exactly.
    converter::OMETIFFJp2KConverterParameters parameters;
    parameters.setCompressionRate(1.f);
    // Several reader threads share the one computed scene; the source does not
    // support concurrent reads, so CVScene serialises them.
    parameters.setNumReadingThreads(4);
    ASSERT_NO_THROW(converter::convertScene(scene, parameters, outPath, 4));
    ASSERT_TRUE(std::filesystem::exists(outPath));

    {
        auto reopened = openSlide(outPath, "AUTO");
        ASSERT_NE(nullptr, reopened);
        auto readBack = reopened->getScene(0);
        ASSERT_NE(nullptr, readBack);
        EXPECT_EQ(512, std::get<2>(readBack->getRect()));
        EXPECT_EQ(512, std::get<3>(readBack->getRect()));
        EXPECT_EQ(3, readBack->getNumChannels());

        const int width = 512;
        const int height = 512;
        const int channels = 3;
        std::vector<uint8_t> buffer(static_cast<size_t>(width) * height * channels);
        readBack->readBlock(std::make_tuple(0, 0, width, height), buffer.data(), buffer.size());
        int mismatches = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    const uint8_t actual = buffer[(static_cast<size_t>(y) * width + x) * channels + c];
                    if (actual != TestSceneSource::expected(x, y, c)) {
                        if (mismatches == 0) {
                            EXPECT_EQ(TestSceneSource::expected(x, y, c), actual)
                                << "first mismatch at x=" << x << " y=" << y << " c=" << c;
                        }
                        ++mismatches;
                    }
                }
            }
        }
        EXPECT_EQ(0, mismatches);
    }

    std::filesystem::remove(outPath);
}

namespace
{
    // Channel 0 is bytes, channel 1 is 16-bit with value 1000 + x.
    class MixedTypeSource : public ISceneSource
    {
    public:
        void readBlock(const Rect& rect, const Size& blockSize, const std::vector<int>& channels,
                       int, int, void* buffer, size_t) override {
            ASSERT_EQ(std::vector<int>{1}, channels);
            ASSERT_EQ(rect.width, blockSize.width);
            uint16_t* out = static_cast<uint16_t*>(buffer);
            for (int row = 0; row < blockSize.height; ++row) {
                for (int col = 0; col < blockSize.width; ++col) {
                    *out++ = static_cast<uint16_t>(1000 + rect.x + col);
                }
            }
        }
    };
}

TEST(SourceSceneConversion, aBorderTileOfANonFirstChannelTypeKeepsItsPixels)
{
    // The border-tile path typed its tile by channel 0. A 16-bit read copied
    // into an 8-bit tile reallocated the ROI and left the tile zero-filled.
    SceneMetadata m;
    m.rect = Rect(0, 0, 300, 300);
    m.channelDataTypes = {DataType::DT_Byte, DataType::DT_UInt16};
    m.levels.emplace_back(0, Size(300, 300), 1.0, 20.0, Size(256, 256));
    auto scene = SceneInternal::getCVScene(makeScene(std::make_shared<MixedTypeSource>(), m));

    cv::Mat tile;
    converter::ConverterTools::readTile(scene, {1}, 0, cv::Rect(256, 256, 256, 256), 0, 0, tile);
    ASSERT_EQ(CV_16UC1, tile.type());
    ASSERT_EQ(cv::Size(256, 256), tile.size());
    EXPECT_EQ(1256, tile.at<uint16_t>(0, 0));
    EXPECT_EQ(1299, tile.at<uint16_t>(43, 43));
    EXPECT_EQ(0, tile.at<uint16_t>(44, 44));        // outside the scene
}

TEST(SourceSceneConversion, aSourceThatThrowsSurfacesAsAnExceptionFromConvertScene)
{
    auto source = std::make_shared<TestSceneSource>();
    source->setThrowOnRead(true);
    auto scene = makeScene(source, computedSceneMetadata());
    const std::string outPath = tempOutputPath("slideio_source_scene_throw.ome.tiff");

    converter::OMETIFFJp2KConverterParameters parameters;
    parameters.setNumReadingThreads(4);
    EXPECT_THROW(converter::convertScene(scene, parameters, outPath, 4), std::exception);

    std::filesystem::remove(outPath);
}

TEST(SourceSceneConversion, aSceneCloneFailureSurfacesAsAnExceptionNotACrash)
{
    // Before the fix, TiffConverter::readTiles cloned the scene outside its
    // try block, so a clone failure in a reader thread called std::terminate.
    auto source = std::make_shared<TestSceneSource>();
    auto scene = makeScene(source, computedSceneMetadata());
    const std::string outPath = tempOutputPath("slideio_source_scene_clone.ome.tiff");

    converter::OMETIFFJp2KConverterParameters parameters;
    parameters.setNumReadingThreads(4);
    {
        FailingCloneConverter converter;
        ASSERT_NO_THROW(converter.createFileLayout(SceneInternal::getCVScene(scene), parameters));
        EXPECT_THROW(converter.createTiff(outPath, nullptr, 4), RuntimeError);
    }

    std::filesystem::remove(outPath);
}

namespace
{
    // Channel 0 is bytes, channel 1 is 16-bit; both a pure function of the
    // scene coordinate, so a converted file can be checked pixel by pixel.
    class MixedPixelSource : public ISceneSource
    {
    public:
        static uint8_t byteValue(int x, int y) { return static_cast<uint8_t>((x * 3 + y * 5) & 0xFF); }
        static uint16_t wordValue(int x, int y) { return static_cast<uint16_t>(300 + x * 11 + y * 17); }

        void readBlock(const Rect& rect, const Size& blockSize, const std::vector<int>& channels,
                       int, int, void* buffer, size_t) override {
            if (channels.size() != 1) {
                throw std::runtime_error("MixedPixelSource: one channel per read");
            }
            for (int row = 0; row < blockSize.height; ++row) {
                const int y = rect.y + row * rect.height / blockSize.height;
                for (int col = 0; col < blockSize.width; ++col) {
                    const int x = rect.x + col * rect.width / blockSize.width;
                    const size_t i = static_cast<size_t>(row) * blockSize.width + col;
                    if (channels[0] == 0) {
                        static_cast<uint8_t*>(buffer)[i] = byteValue(x, y);
                    }
                    else {
                        static_cast<uint16_t*>(buffer)[i] = wordValue(x, y);
                    }
                }
            }
        }
    };
}

TEST(SourceSceneConversion, aChannelRangeOfAnotherTypeThanChannelZeroIsWrittenWithItsType)
{
    // The OME-XML Pixels Type came from channel 0 whatever channel range was
    // written, so a range of 16-bit channels was declared uint8.
    SceneMetadata m;
    m.name = "mixed";
    m.rect = Rect(0, 0, 300, 200);
    m.channelDataTypes = {DataType::DT_Byte, DataType::DT_UInt16};
    m.levels.emplace_back(0, Size(300, 200), 1.0, 20.0, Size(256, 256));
    auto scene = makeScene(std::make_shared<MixedPixelSource>(), m);
    const std::string outPath = tempOutputPath("slideio_source_scene_range.ome.tiff");

    converter::OMETIFFJp2KConverterParameters parameters;
    parameters.setCompressionRate(1.f);                 // lossless
    parameters.setChannelRange(Range(1, 2));
    ASSERT_NO_THROW(converter::convertScene(scene, parameters, outPath, 4));

    {
        auto reopened = openSlide(outPath, "AUTO");
        ASSERT_NE(nullptr, reopened);
        auto readBack = reopened->getScene(0);
        ASSERT_NE(nullptr, readBack);
        ASSERT_EQ(1, readBack->getNumChannels());
        EXPECT_EQ(DataType::DT_UInt16, readBack->getChannelDataType(0));
        if (readBack->getChannelDataType(0) == DataType::DT_UInt16) {
            std::vector<uint16_t> buffer(300 * 200);
            readBack->readBlock(std::make_tuple(0, 0, 300, 200),
                                reinterpret_cast<uint8_t*>(buffer.data()),
                                buffer.size() * sizeof(uint16_t));
            int mismatches = 0;
            for (int y = 0; y < 200; ++y) {
                for (int x = 0; x < 300; ++x) {
                    const uint16_t actual = buffer[static_cast<size_t>(y) * 300 + x];
                    if (actual != MixedPixelSource::wordValue(x, y)) {
                        if (mismatches == 0) {
                            EXPECT_EQ(MixedPixelSource::wordValue(x, y), actual)
                                << "first mismatch at x=" << x << " y=" << y;
                        }
                        ++mismatches;
                    }
                }
            }
            EXPECT_EQ(0, mismatches);
        }
    }

    std::filesystem::remove(outPath);
}

namespace
{
    // Converts a computed 512x512, 3 x 8-bit scene to a lossless OME-TIFF, so
    // a test has a file-backed scene without shipping a test image.
    std::string writeComputedOmeTiff(const char* name)
    {
        auto scene = makeScene(std::make_shared<TestSceneSource>(), computedSceneMetadata());
        const std::string path = tempOutputPath(name);
        converter::OMETIFFJp2KConverterParameters parameters;
        parameters.setCompressionRate(1.f);
        converter::convertScene(scene, parameters, path, 4);
        return path;
    }

    // A file-backed-looking scene whose path, driver, scene index and shape a
    // test chooses, to drive TiffConverter::cloneScene's re-open decision.
    class ReopenableScene : public TestScene
    {
    public:
        ReopenableScene(const std::string& path, int sceneIndex)
            : m_sceneIndex(sceneIndex), m_driver("OMETIFF")
        {
            setFilePath(path);
            setRect(cv::Rect(0, 0, 512, 512));
            setNumChannels(3);
            setChannelDataType(DataType::DT_Byte);
        }
        int getSceneIndex() const override { return m_sceneIndex; }
        const std::string& getDriverId() const override { return m_driver; }
    private:
        int m_sceneIndex;
        std::string m_driver;
    };

    std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> cloneOf(
        const std::shared_ptr<CVScene>& scene)
    {
        converter::OMETIFFJp2KConverterParameters parameters;
        converter::TiffConverter converter;
        converter.createFileLayout(scene, parameters);
        return converter.cloneScene();
    }

    bool isShared(const std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>>& clone,
                  const std::shared_ptr<CVScene>& scene)
    {
        return clone.first == nullptr && clone.second != nullptr
            && SceneInternal::getCVScene(clone.second) == scene;
    }
}

TEST(SourceSceneConversion, aSceneMatchingItsReopenedFileIsReopened)
{
    const std::string path = writeComputedOmeTiff("slideio_clone_match.ome.tiff");
    {
        auto scene = std::make_shared<ReopenableScene>(path, 0);
        auto clone = cloneOf(scene);
        ASSERT_NE(nullptr, clone.first);                 // a slide of its own
        ASSERT_NE(nullptr, clone.second);
        EXPECT_NE(std::static_pointer_cast<CVScene>(scene), SceneInternal::getCVScene(clone.second));
    }
    std::filesystem::remove(path);
}

TEST(SourceSceneConversion, aSceneWithANegativeIndexIsSharedNotReopened)
{
    // An auxiliary image (a CZI thumbnail, say) reports index -1: there is no
    // getScene(-1) to re-open it by.
    const std::string path = writeComputedOmeTiff("slideio_clone_negative.ome.tiff");
    {
        auto scene = std::make_shared<ReopenableScene>(path, -1);
        std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> clone;
        ASSERT_NO_THROW(clone = cloneOf(scene));
        EXPECT_TRUE(isShared(clone, scene));
    }
    std::filesystem::remove(path);
}

TEST(SourceSceneConversion, aSceneWhoseReopenFailsIsShared)
{
    const std::string path = tempOutputPath("slideio_clone_missing.ome.tiff");   // never written
    auto scene = std::make_shared<ReopenableScene>(path, 0);
    std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> clone;
    ASSERT_NO_THROW(clone = cloneOf(scene));
    EXPECT_TRUE(isShared(clone, scene));
}

TEST(SourceSceneConversion, aSceneThatDiffersFromItsReopenedFileIsShared)
{
    // Re-opening by (path, driver, index) succeeds but yields another raster:
    // the converter would silently write that one instead.
    const std::string path = writeComputedOmeTiff("slideio_clone_mismatch.ome.tiff");
    {
        auto otherRect = std::make_shared<ReopenableScene>(path, 0);
        otherRect->setRect(cv::Rect(0, 0, 256, 512));
        EXPECT_TRUE(isShared(cloneOf(otherRect), otherRect));

        auto otherChannels = std::make_shared<ReopenableScene>(path, 0);
        otherChannels->setNumChannels(1);
        EXPECT_TRUE(isShared(cloneOf(otherChannels), otherChannels));

        auto otherType = std::make_shared<ReopenableScene>(path, 0);
        otherType->setChannelDataType(DataType::DT_UInt16);
        EXPECT_TRUE(isShared(cloneOf(otherType), otherType));
    }
    std::filesystem::remove(path);
}
