// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Indexing the nine real integration bundles (design section 9). Needs the image corpus
// under SLIDEIO_IMAGES_PATH, hence the MRXSCorpus suite name that CI's filter excludes.
// Expected level and record counts are those measured in the format description,
// section 17.
#include <gtest/gtest.h>
#include <string>
#include "slideio/drivers/mrxs/mrxsindex.hpp"
#include "slideio/core/exceptions.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio::mrxs;

namespace
{
    struct Bundle
    {
        const char* folder;
        const char* entry;
        size_t levels;
        size_t records;     // summed over all stored levels
    };

    const Bundle kBundles[] = {
        {"mirax/openslide/CMU-1-Saved-1_16", "CMU-1-Saved-1_16.mrxs", 6, 514},
        {"mirax/openslide/CMU-1-Saved-1_2", "CMU-1-Saved-1_2.mrxs", 9, 13986},
        {"mirax/openslide/Mirax2-Fluorescence-1", "Mirax2-Fluorescence-1.mrxs", 10, 2541},
        {"mirax/openslide/Mirax2-Fluorescence-2", "Mirax2-Fluorescence-2.mrxs", 10, 1709},
        {"mirax/openslide/Mirax2.2-4-PNG", "Mirax2.2-4-PNG.mrxs", 10, 7190},
        {"mirax/openslide/Mirax2.2-4-BMP", "Mirax2.2-4-BMP.mrxs", 10, 7190},
        {"mirax/zenodo/20240920152150628", "20240920152150628.mrxs", 10, 14293},
        {"mirax/zenodo/PDX_H5097_ID_2053_LG_HE", "PDX_H5097_ID_2053_LG_HE.mrxs", 9, 80982},
    };
}

TEST(MRXSCorpusIndex, integrationBundlesIndexCompletely)
{
    for (const Bundle& bundle : kBundles) {
        SCOPED_TRACE(bundle.entry);
        const std::string path = TestTools::getTestImagePath(bundle.folder, bundle.entry);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        const SlideStructure slide = readSlideStructure(path);
        EXPECT_EQ(bundle.levels, slide.levels.size());
        size_t records = 0;
        for (const auto& level : slide.levels) {
            records += level.size();
        }
        EXPECT_EQ(bundle.records, records);
        EXPECT_EQ(3u, slide.associated.size());
    }
}

TEST(MRXSCorpusIndex, positionSourcesMatchTheFormatDescription)
{
    const struct { const char* folder; const char* entry; PositionSource source; } cases[] = {
        {"mirax/openslide/CMU-1-Saved-1_16", "CMU-1-Saved-1_16.mrxs", PositionSource::Raw},
        {"mirax/openslide/Mirax2.2-4-PNG", "Mirax2.2-4-PNG.mrxs", PositionSource::Zlib},
        {"mirax/zenodo/20240920152150628", "20240920152150628.mrxs", PositionSource::None},
        {"mirax/zenodo/PDX_H5097_ID_2053_LG_HE", "PDX_H5097_ID_2053_LG_HE.mrxs", PositionSource::Zlib},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.entry);
        const std::string path = TestTools::getTestImagePath(c.folder, c.entry);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        EXPECT_EQ(c.source, readSlideStructure(path).positionSource);
    }
}

TEST(MRXSCorpusIndex, theFiveChannelBundleIsUnsupported)
{
    const std::string path = TestTools::getTestImagePath("mirax/zenodo/2401582-38_mihc", "2401582-38.mrxs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::string message;
    try {
        readSlideStructure(path);
    }
    catch (const slideio::RuntimeError& e) {
        message = e.what();
    }
    EXPECT_EQ(0u, message.find("MRXS unsupported:")) << message;
}
