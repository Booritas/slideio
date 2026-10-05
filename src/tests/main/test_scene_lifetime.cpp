// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// A scene must stay fully usable after the slide that opened it is released: Python code
// that keeps a scene and drops the slide is ordinary. Scenes in GDAL, NDPI and CZI used to
// reach the file or the metadata through a raw pointer into the slide, and crashed, threw
// or read freed memory once it was gone. Every scene and auxiliary image of one file per
// driver is read before and after the release, and the two must agree.
#include <gtest/gtest.h>
#include <algorithm>
#include <functional>
#include <string>
#include <tuple>
#include <vector>
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/slide.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

namespace
{
    // What a scene reports and what it reads: everything that must not change when the
    // slide goes away.
    struct SceneState
    {
        std::string name;
        std::tuple<int, int, int, int> rect;
        int numChannels = 0;
        std::tuple<double, double> resolution;
        double magnification = 0.;
        double zResolution = 0.;
        double tResolution = 0.;
        std::vector<uint8_t> block;

        bool operator==(const SceneState& other) const
        {
            return name == other.name && rect == other.rect && numChannels == other.numChannels
                && resolution == other.resolution && magnification == other.magnification
                && zResolution == other.zResolution && tResolution == other.tResolution
                && block == other.block;
        }
    };

    SceneState capture(Scene& scene)
    {
        SceneState state;
        state.name = scene.getName();
        state.rect = scene.getRect();
        state.numChannels = scene.getNumChannels();
        state.resolution = scene.getResolution();
        state.magnification = scene.getMagnification();
        state.zResolution = scene.getZSliceResolution();
        state.tResolution = scene.getTFrameResolution();
        const int w = std::get<2>(state.rect);
        const int h = std::get<3>(state.rect);
        const int bw = std::min(w, 64);
        const int bh = std::min(h, 64);
        state.block.resize(scene.getBlockSize({bw, bh}, 0, state.numChannels, 1, 1));
        scene.readBlock({w / 2 - bw / 2, h / 2 - bh / 2, bw, bh}, state.block.data(), state.block.size());
        return state;
    }

    using ScenePicker = std::function<std::shared_ptr<Scene>(const std::shared_ptr<Slide>&)>;

    // Every main scene, every scene's auxiliary images and the slide's auxiliary images.
    std::vector<std::pair<std::string, ScenePicker>> listScenes(const std::shared_ptr<Slide>& slide)
    {
        std::vector<std::pair<std::string, ScenePicker>> pickers;
        for (int index = 0; index < slide->getNumScenes(); ++index) {
            pickers.emplace_back("scene " + std::to_string(index),
                [index](const std::shared_ptr<Slide>& s) { return s->getScene(index); });
            for (const auto& name : slide->getScene(index)->getAuxImageNames()) {
                pickers.emplace_back("scene " + std::to_string(index) + " aux " + name,
                    [index, name](const std::shared_ptr<Slide>& s) { return s->getScene(index)->getAuxImage(name); });
            }
        }
        for (const auto& name : slide->getAuxImageNames()) {
            pickers.emplace_back("slide aux " + name,
                [name](const std::shared_ptr<Slide>& s) { return s->getAuxImage(name); });
        }
        return pickers;
    }

    void expectScenesOutliveTheirSlide(const std::string& path, const std::string& driver)
    {
        const auto pickers = listScenes(openSlide(path, driver));
        ASSERT_FALSE(pickers.empty());
        for (const auto& picker : pickers) {
            SCOPED_TRACE(picker.first);
            // A fresh slide per scene, so that releasing it really releases the last
            // reference: no earlier scene is left holding the file open.
            std::shared_ptr<Slide> slide = openSlide(path, driver);
            std::shared_ptr<Scene> scene = picker.second(slide);
            const SceneState before = capture(*scene);
            slide.reset();
            const SceneState after = capture(*scene);
            EXPECT_TRUE(before == after);
        }
    }
}

TEST(SceneLifetime, svs)
{
    const std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "SVS");
}

TEST(SceneLifetime, afi)
{
    const std::string path = TestTools::getTestImagePath("afi", "fs.afi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "AFI");
}

TEST(SceneLifetime, ndpi)
{
    const std::string path = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "NDPI");
}

TEST(SceneLifetime, scn)
{
    const std::string path = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "SCN");
}

TEST(SceneLifetime, czi)
{
    const std::string path = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.czi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "CZI");
}

TEST(SceneLifetime, zvi)
{
    const std::string path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "ZVI");
}

TEST(SceneLifetime, dcm)
{
    const std::string path = TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "DCM");
}

TEST(SceneLifetime, phtiff)
{
    const std::string path = TestTools::getTestImagePath("philips", "Philips-4.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "PHTIFF");
}

TEST(SceneLifetime, ometiff)
{
    const std::string path = TestTools::getTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "OMETIFF");
}

TEST(SceneLifetime, gdal)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "GDAL");
}

TEST(SceneLifetime, qptiff)
{
    const std::string path = TestTools::getTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "QPTIFF");
}

TEST(SceneLifetime, vsi)
{
    const std::string path = TestTools::getTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectScenesOutliveTheirSlide(path, "VSI");
}
