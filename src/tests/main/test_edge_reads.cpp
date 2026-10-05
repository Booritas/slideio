// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Issue #45: a block read that crosses the scene edge must behave the same in every
// driver -- the block has the size asked for, the part inside the scene carries the
// same pixels an in-bounds read would, and the rest is background. These cases cover
// the read paths that used to crop the raster with the raw rectangle and throw an
// OpenCV ROI assertion, plus NDPI, the format the issue was filed against.
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <algorithm>
#include <string>
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/core/cvscene.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

namespace
{
    // The slide is kept alive alongside the scene: a scene does not own everything its
    // reads depend on.
    struct OpenScene
    {
        std::shared_ptr<Slide> slide;
        std::shared_ptr<CVScene> scene;
    };

    OpenScene openScene(const std::string& path, const std::string& driver)
    {
        auto slide = openSlide(path, driver);
        return {slide, SceneInternal::getCVScene(slide->getScene(0))};
    }

    OpenScene openSlideAux(const std::string& path, const std::string& driver, const std::string& auxName)
    {
        auto slide = openSlide(path, driver);
        return {slide, SceneInternal::getCVScene(slide->getAuxImage(auxName))};
    }

    bool isBackground(const cv::Mat& raster)
    {
        const double background = raster.depth() == CV_8U ? 255. : 0.;
        cv::Mat diff;
        cv::absdiff(raster.reshape(1), cv::Scalar(background), diff);
        return cv::countNonZero(diff) == 0;
    }

    void expectEdgeContract(const std::shared_ptr<CVScene>& scene)
    {
        ASSERT_TRUE(scene != nullptr);
        const cv::Rect sceneRect = scene->getRect();
        const int w = sceneRect.width;
        const int h = sceneRect.height;
        const int a = std::max(8, std::min({w, h, 4000}) / 4);
        SCOPED_TRACE(scene->getName() + " " + std::to_string(w) + "x" + std::to_string(h));

        // Right edge, 1:1: the left half lies inside, the right half outside.
        cv::Mat straddle;
        ASSERT_NO_THROW(scene->readBlock(cv::Rect(w - a, 0, 2 * a, a), straddle));
        ASSERT_EQ(cv::Size(2 * a, a), straddle.size());
        cv::Mat inside;
        scene->readBlock(cv::Rect(w - a, 0, a, a), inside);
        ASSERT_EQ(inside.type(), straddle.type());
        cv::Mat diff;
        cv::absdiff(straddle(cv::Rect(0, 0, a, a)), inside, diff);
        EXPECT_EQ(0, cv::countNonZero(diff.reshape(1))) << "the inside half differs from an in-bounds read";
        EXPECT_TRUE(isBackground(straddle(cv::Rect(a, 0, a, a)))) << "the outside half is not background";

        // Bottom edge and a negative origin: the outside part is background.
        cv::Mat bottom;
        ASSERT_NO_THROW(scene->readBlock(cv::Rect(0, h - a, a, 2 * a), bottom));
        ASSERT_EQ(cv::Size(a, 2 * a), bottom.size());
        EXPECT_TRUE(isBackground(bottom(cv::Rect(0, a, a, a))));
        cv::Mat negative;
        ASSERT_NO_THROW(scene->readBlock(cv::Rect(-a, -a, 2 * a, 2 * a), negative));
        ASSERT_EQ(cv::Size(2 * a, 2 * a), negative.size());
        EXPECT_TRUE(isBackground(negative(cv::Rect(0, 0, 2 * a, a))));
        EXPECT_TRUE(isBackground(negative(cv::Rect(0, a, a, a))));

        // The read from the issue: a large rectangle over the edge, downscaled.
        cv::Mat scaled;
        ASSERT_NO_THROW(scene->readResampledBlock(cv::Rect(w - a, 0, 10 * a, 10 * a), cv::Size(100, 100), scaled));
        EXPECT_EQ(cv::Size(100, 100), scaled.size());

        // Entirely outside: not an error, all background.
        cv::Mat outside;
        ASSERT_NO_THROW(scene->readBlock(cv::Rect(w + a, 0, a, a), outside));
        ASSERT_EQ(cv::Size(a, a), outside.size());
        EXPECT_TRUE(isBackground(outside));
    }
}

TEST(EdgeReads, gdal8bit)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openScene(path, "GDAL").scene);
}

TEST(EdgeReads, gdal16bit)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openScene(path, "GDAL").scene);
}

TEST(EdgeReads, dcm8bit)
{
    const std::string path = TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openScene(path, "DCM").scene);
}

TEST(EdgeReads, dcm16bit)
{
    const std::string path = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openScene(path, "DCM").scene);
}

TEST(EdgeReads, svsStripedAuxImage)
{
    const std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openSlideAux(path, "SVS", "Thumbnail").scene);
}

TEST(EdgeReads, pkeStripedAuxImage)
{
    const std::string path = TestTools::getTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openSlideAux(path, "QPTIFF", "Thumbnail").scene);
}

TEST(EdgeReads, cziThumbnail)
{
    const std::string path = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.czi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openSlideAux(path, "CZI", "Thumbnail").scene);
}

TEST(EdgeReads, vsiFileScene)
{
    const std::string path = TestTools::getTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto opened = openScene(path, "VSI");
    ASSERT_FALSE(opened.scene->getAuxImageNames().empty());
    expectEdgeContract(opened.scene->getAuxImage(opened.scene->getAuxImageNames().front()));
}

TEST(EdgeReads, ndpi)
{
    const std::string path = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    expectEdgeContract(openScene(path, "NDPI").scene);
}
