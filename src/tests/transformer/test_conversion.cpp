// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/slideio/scenesource.hpp"
#include "slideio/slideio/scenemetadata.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/colortransformation.hpp"
#include "tests/testlib/testscene.hpp"
#include "tests/testlib/testscenesource.hpp"

using namespace slideio;

namespace
{
    // Prefixed with a token drawn once per process, so concurrent runs of the
    // suite on one machine (ctest -j, two CI jobs) never share a file.
    std::string tempOutputPath(const char* name)
    {
        static const std::string token = std::to_string(std::random_device{}()) + "_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::string path =
            (std::filesystem::temp_directory_path() / (token + "_" + name)).string();
        std::filesystem::remove(path);
        return path;
    }

    // A file-backed scene with no test image: a computed 512x512 RGB scene
    // converted to a lossless OME-TIFF.
    std::string writeComputedOmeTiff(const char* name)
    {
        SceneMetadata m;
        m.name = "computed";
        m.rect = Rect(0, 0, 512, 512);
        m.channelDataTypes.assign(3, DataType::DT_Byte);
        m.resolution = Resolution(2.5e-7, 2.5e-7);
        m.magnification = 20.0;
        m.levels.emplace_back(0, Size(512, 512), 1.0, 20.0, Size(256, 256));
        auto scene = makeScene(std::make_shared<TestSceneSource>(), m);

        const std::string path = tempOutputPath(name);
        converter::OMETIFFJp2KConverterParameters parameters;
        parameters.setCompressionRate(1.f);            // lossless
        converter::convertScene(scene, parameters, path, 4);
        return path;
    }

    std::shared_ptr<Scene> grayTransformOf(const std::shared_ptr<Scene>& origin)
    {
        ColorTransformation gray;
        gray.setColorSpace(ColorSpace::GRAY);
        return transformScene(origin, gray);
    }

    std::vector<uint8_t> readAll(const std::shared_ptr<Scene>& scene, int width, int height)
    {
        std::vector<uint8_t> buffer(static_cast<size_t>(width) * height * scene->getNumChannels());
        scene->readBlock(std::make_tuple(0, 0, width, height), buffer.data(), buffer.size());
        return buffer;
    }
}

TEST(TransformerSceneConversion, aTransformedSceneIsNeverReopenedByPath)
{
    // Re-opening a transform's (path, driver, index) yields its untransformed
    // origin, so the converter must share the transform instead.
    auto origin = std::make_shared<TestScene>();
    origin->setNumChannels(3);
    origin->setChannelDataType(DataType::DT_Byte);
    ASSERT_TRUE(origin->canReopenByPath());
    auto transformed = grayTransformOf(SceneInternal::createScene(origin));
    EXPECT_FALSE(SceneInternal::getCVScene(transformed)->canReopenByPath());
}

TEST(TransformerSceneConversion, eachReaderGetsItsOwnTransformOverAReopenedOrigin)
{
    // Sharing one transform made a conversion over a non-concurrent origin
    // single-threaded. Each reader instead re-opens the origin and gets the
    // same transformations re-created over it.
    const std::string originPath = writeComputedOmeTiff("slideio_transform_clone.ome.tiff");
    {
        auto originSlide = openSlide(originPath, "AUTO");
        ASSERT_NE(nullptr, originSlide);
        auto origin = originSlide->getScene(0);
        auto transformed = grayTransformOf(origin);
        auto cvTransformed = SceneInternal::getCVScene(transformed);

        converter::OMETIFFJp2KConverterParameters parameters;
        converter::TiffConverter converter;
        converter.createFileLayout(cvTransformed, parameters);
        auto clone = converter.cloneScene();
        ASSERT_NE(nullptr, clone.first);                  // a slide of its own
        ASSERT_NE(nullptr, clone.second);
        auto cvClone = SceneInternal::getCVScene(clone.second);
        EXPECT_NE(cvTransformed, cvClone);
        ASSERT_NE(nullptr, cvClone->getOriginScene());
        EXPECT_NE(SceneInternal::getCVScene(origin), cvClone->getOriginScene());
        EXPECT_EQ(1, cvClone->getNumChannels());          // still the gray transform
        EXPECT_EQ(readAll(transformed, 512, 512), readAll(clone.second, 512, 512));
    }
    std::filesystem::remove(originPath);
}

TEST(TransformerSceneConversion, aTransformOfAFileBackedSceneConvertsTransformed)
{
    const std::string originPath = writeComputedOmeTiff("slideio_transform_origin.ome.tiff");
    const std::string outPath = tempOutputPath("slideio_transform_output.ome.tiff");
    {
        auto originSlide = openSlide(originPath, "AUTO");
        ASSERT_NE(nullptr, originSlide);
        auto origin = originSlide->getScene(0);
        ASSERT_NE(nullptr, origin);
        auto transformed = grayTransformOf(origin);
        ASSERT_EQ(1, transformed->getNumChannels());

        converter::OMETIFFJp2KConverterParameters parameters;
        parameters.setCompressionRate(1.f);            // lossless
        parameters.setNumReadingThreads(4);            // several readers share the transform
        ASSERT_NO_THROW(converter::convertScene(transformed, parameters, outPath, 4));

        auto converted = openSlide(outPath, "AUTO");
        ASSERT_NE(nullptr, converted);
        auto readBack = converted->getScene(0);
        ASSERT_NE(nullptr, readBack);
        ASSERT_EQ(1, readBack->getNumChannels());

        const std::vector<uint8_t> expected = readAll(transformed, 512, 512);
        const std::vector<uint8_t> raw = readAll(origin, 512, 512);
        const std::vector<uint8_t> actual = readAll(readBack, 512, 512);

        // The transform must change the pixels, or this test proves nothing:
        // gray differs from the origin's first channel.
        int differFromRaw = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
            if (expected[i] != raw[i * 3]) {
                ++differFromRaw;
            }
        }
        ASSERT_GT(differFromRaw, 0);

        int mismatches = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
            if (expected[i] != actual[i]) {
                if (mismatches == 0) {
                    EXPECT_EQ(expected[i], actual[i]) << "first mismatch at pixel " << i
                        << " (untransformed channel 0 there is " << int(raw[i * 3]) << ")";
                }
                ++mismatches;
            }
        }
        EXPECT_EQ(0, mismatches);
    }
    std::filesystem::remove(outPath);
    std::filesystem::remove(originPath);
}
