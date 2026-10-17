// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "slideio/slideio/slideio.hpp"
#include "slideio/core/exceptions.hpp"
#include "tests/testlib/testtools.hpp"

namespace
{
    struct DriverImage
    {
        const char* driver;
        const char* subfolder;
        const char* image;
    };

    std::ostream& operator<<(std::ostream& out, const DriverImage& d)
    {
        return out << d.driver;
    }

    class SceneIndex : public ::testing::TestWithParam<DriverImage> {};
}

// Issue #87: a negative index reached the driver's scene vector, an
// out-of-bounds read, instead of raising like an index past the end. Every
// driver serves its own getScene, so every driver is checked.
TEST_P(SceneIndex, anIndexOutsideTheSlideRaisesNamingTheIndex)
{
    const DriverImage& d = GetParam();
    const std::string path = TestTools::getTestImagePath(d.subfolder, d.image);
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto slide = slideio::openSlide(path, d.driver);
    ASSERT_NE(nullptr, slide);
    const int numScenes = slide->getNumScenes();
    ASSERT_GT(numScenes, 0);
    EXPECT_NE(nullptr, slide->getScene(numScenes - 1));
    for (const int index : {-1, -1000, numScenes}) {
        try {
            slide->getScene(index);
            ADD_FAILURE() << "scene index " << index << " of " << numScenes << " was accepted";
        }
        catch (const slideio::RuntimeError& e) {
            const std::string what = e.what();
            EXPECT_NE(std::string::npos, what.find("invalid scene index: " + std::to_string(index)))
                << what;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllDrivers, SceneIndex,
    ::testing::Values(
        DriverImage{"AFI", "afi", "fs.afi"},
        DriverImage{"CZI", "czi", "pJP31mCherry.czi"},
        DriverImage{"DCM", "dcm", "barre.dev/OT-MONO2-8-hip.dcm"},
        DriverImage{"GDAL", "gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png"},
        DriverImage{"NDPI", "hamamatsu", "openslide/CMU-1.ndpi"},
        DriverImage{"OMETIFF", "ometiff", "Multifile/multifile-Z1.ome.tiff"},
        DriverImage{"PHTIFF", "philips", "Philips-3.tiff"},
        DriverImage{"QPTIFF", "pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff"},
        DriverImage{"SCN", "scn", "ultivue/Leica Aperio Versa 5 channel fluorescent image.scn"},
        DriverImage{"SVS", "svs", "CMU-1-Small-Region.svs"},
        DriverImage{"VSI", "vsi", "OS-1/OS-1.vsi"},
        DriverImage{"ZVI", "zvi", "Zeiss-1-Stacked.zvi"}),
    [](const ::testing::TestParamInfo<DriverImage>& info) { return std::string(info.param.driver); });
