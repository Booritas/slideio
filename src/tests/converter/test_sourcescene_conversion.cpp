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
#include "tests/testlib/testscenesource.hpp"
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
