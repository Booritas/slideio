// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Separate from test_ometiff_driver.cpp on purpose: this task must not touch that file
// (it carries unrelated uncommitted work from another task in flight on this branch).
#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"

using namespace slideio;
using namespace slideio::ometiff;

TEST(OTImageDriverColorProfile, colorProfileAbsentWhenTiffTagIsAbsent)
{
    // No OME-TIFF image in the corpus available to this task carries an ICC tag
    // (checked with a raw TIFF IFD walker over the whole images corpus).
    std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    OTImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const ColorProfile profile = scene->getColorProfile();
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
}
