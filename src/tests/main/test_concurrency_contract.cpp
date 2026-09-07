// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "tests/testlib/testscene.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/drivers/zvi/zviimagedriver.hpp"
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"

// TestScene is the in-tree fake CVScene used by the tile-composer tests. It does
// not override supportsConcurrentReads, so it must report the safe default.
TEST(ConcurrencyContract, defaultsToSerialised) {
    TestScene scene;
    EXPECT_FALSE(scene.supportsConcurrentReads());
}

namespace
{
    class ConcurrentTestScene : public TestScene
    {
    public:
        bool supportsConcurrentReads() const override { return true; }
    };
}

TEST(ConcurrencyContract, driverCanOptIn) {
    ConcurrentTestScene scene;
    EXPECT_TRUE(scene.supportsConcurrentReads());
}

namespace
{
    // Asserts a deferred driver's first scene still reports the serialised default.
    // If this ever fails, it means an earlier change opted this driver into
    // concurrent reads without updating the documentation -- do that (TECH_DEBT,
    // BREAKING_CHANGES.md, CLAUDE.md) and give it a byte-exactness test before
    // changing the expectation this test enforces.
    void expectSerialised(const std::string& filePath, slideio::ImageDriver& driver) {
        auto slide = driver.openFile(filePath);
        ASSERT_TRUE(slide);
        auto scene = slide->getScene(0);
        ASSERT_TRUE(scene);
        EXPECT_FALSE(scene->supportsConcurrentReads())
            << "this driver now reports concurrent reads -- update TECH_DEBT, "
               "BREAKING_CHANGES.md and CLAUDE.md, and give it a byte-exactness "
               "test, before changing this expectation";
    }
}

// Image paths mirror the multiThreadedTest call sites in test_zvi_driver.cpp,
// test_dcm_driver.cpp and test_gdal_driver.cpp, since those are already known
// to open successfully with each of these drivers.

TEST(ConcurrencyContract, zviIsStillSerialised) {
    const std::string filePath = TestTools::getTestImagePath(
        "zvi", "mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::ZVIImageDriver driver;
    expectSerialised(filePath, driver);
}

TEST(ConcurrencyContract, dcmIsStillSerialised) {
    const std::string filePath = TestTools::getTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::DCMImageDriver driver;
    expectSerialised(filePath, driver);
}

TEST(ConcurrencyContract, gdalIsStillSerialised) {
    const std::string filePath = TestTools::getTestImagePath(
        "gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::GDALImageDriver driver;
    expectSerialised(filePath, driver);
}
