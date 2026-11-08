// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Indexing the nine real integration bundles (design section 9). Needs the image corpus
// under SLIDEIO_IMAGES_PATH, hence the MRXSCorpus suite name that CI's filter excludes.
// Expected level and record counts are those measured in the format description,
// section 17.
//
// The bundles these suites were qualified on (2026-11-07), identified by SHA-256 of the
// two files that fix every payload's location. Data files are too large to hash here;
// a bundle whose INI or index differs is a different bundle.
//   openslide/CMU-1-Saved-1_16           Slidedat.ini 9f9e494ae69d3ba43d71cef8bdf85f723c0517d7321115fdefa7a7ebef7a5daa
//                                        Index.dat    8e8c9857a70379e0355754abc040b6e9532797b768ffc8c085753e790d17fc17
//   openslide/CMU-1-Saved-1_2            Slidedat.ini 575c721ca332d3826e8dc634e1c393b93c902324fbbc0ee8d52921ef6731ac33
//                                        Index.dat    6bf58b06c860b8959d4b7f4963d051dc19127ae716730cc906b28808a356438d
//   openslide/Mirax2-Fluorescence-1      Slidedat.ini b0e91c582d644b014fee7eafdad66a6535a867b6744a2da6843e6b7ceb0ccbc7
//                                        Index.dat    625de8de9dd518ba239c830c29edb8f16890fb52c101ba66eae3233c418dc901
//   openslide/Mirax2-Fluorescence-2      Slidedat.ini 394849774da325e3409d35d5be940877006b54a1958e19a1c6bf2a125aa2dae7
//                                        Index.dat    bb9bd406b5c806651f462e5bc3a541f3340836d6b0e756f2ea805bbaa8fae0dd
//   openslide/Mirax2.2-4-PNG             Slidedat.ini 39131b5ce4af0accf6fdd81720f23d291699d0bace674e3f45c5bf0507d0fffe
//                                        Index.dat    58d6a0264f70b727678b2fa0c8b3d4f4026ec632dde485683ca7bf1cca23af79
//   openslide/Mirax2.2-4-BMP             Slidedat.ini f25ba62f8762a1eda2c1f19394417dc1ebc2e26e70f0a8075437684709abb7e2
//                                        Index.dat    83cd6153ae49e6933acb62b69c2ad70522eb7dcce665cea061ba628bc621ec5a
//   zenodo/20240920152150628             Slidedat.ini dc0126ab69f3a99e06ffa8c8e9aac8483c7c4a6ef20bc980cf1e3a55f6489c31
//                                        Index.dat    cbeb9bbf183b9fff8f5f71aa5c980ef39690861af0a5f9259c5f1b7c287be5fe
//   zenodo/PDX_H5097_ID_2053_LG_HE       Slidedat.ini 308b068c79fa14e0c4ce1257fccce7868a9037a3ea39ecd29add6caab2bf5c00
//                                        Index.dat    216d714c2c9453fc00fd72f427b19c1e85da7ea7a0c5d10463b8fb01f3b1fcfb
//   zenodo/2401582-38_mihc               Slidedat.ini d9f6d0be4aacff976a0c1ba195d3c3e28d0e9f882fc94805b4a627735d762780
//                                        Index.dat    6728e52a968b2a5ffa4f9a7d273dda5021cdf7ca03d67abf78989b3eceb094ab
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
