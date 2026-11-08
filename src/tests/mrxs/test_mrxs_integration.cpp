// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// End to end through the public API (design sections 2 and 10): registration, AUTO
// detection of both entry kinds, letter case, Unicode and extracted bundles, the error a
// corrupt bundle reports under AUTO, and conversion of an MRXS scene.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tempfile.hpp"

namespace fs = std::filesystem;

namespace
{
    const fs::path dataDir() { return fs::path(MRXS_TEST_DATA_DIR); }

    std::string fixture(const std::string& relative) { return (dataDir() / fs::u8path(relative)).u8string(); }

    // A scratch directory removed when the test ends.
    class ScratchDir
    {
    public:
        ScratchDir() : m_path(fs::temp_directory_path() / slideio::TempFile::unique_path("mrxs-%%%%-%%%%")) {
            fs::create_directories(m_path);
        }
        ~ScratchDir() { std::error_code ec; fs::remove_all(m_path, ec); }
        const fs::path& path() const { return m_path; }
    private:
        fs::path m_path;
    };

    // Copies fixture `name` (entry file and data directory) under `dir` as `newName`.
    fs::path copyBundle(const std::string& name, const fs::path& dir, const fs::path& newName)
    {
        fs::copy(dataDir() / name, dir / newName, fs::copy_options::recursive);
        fs::path entry = dir / newName;
        entry += ".mrxs";
        fs::copy_file(dataDir() / (name + ".mrxs"), entry);
        return entry;
    }

    std::vector<uint8_t> readAll(const std::shared_ptr<slideio::Scene>& scene)
    {
        const auto rect = scene->getRect();
        std::vector<uint8_t> pixels(size_t(std::get<2>(rect)) * std::get<3>(rect) * scene->getNumChannels());
        scene->readBlock(rect, pixels.data(), pixels.size());
        return pixels;
    }

    std::string openError(const std::string& path)
    {
        try {
            slideio::openSlide(path, "");
        }
        catch (const std::exception& e) {
            return e.what();
        }
        return "";
    }
}

TEST(MRXSIntegration, theDriverIsRegistered)
{
    const std::vector<std::string> ids = slideio::getDriverIDs();
    EXPECT_NE(ids.end(), std::find(ids.begin(), ids.end(), "MRXS"));
}

TEST(MRXSIntegration, autoOpensAnMrxsEntry)
{
    const auto slide = slideio::openSlide(fixture("nomap-d1.mrxs"), "");
    EXPECT_EQ("MRXS", slide->getDriverId());
    EXPECT_EQ(1, slide->getNumScenes());
    EXPECT_FALSE(readAll(slide->getScene(0)).empty());
}

TEST(MRXSIntegration, autoOpensSlidedatIni)
{
    const auto byIni = slideio::openSlide(fixture("nomap-d1/Slidedat.ini"), "AUTO");
    const auto byEntry = slideio::openSlide(fixture("nomap-d1.mrxs"), "MRXS");
    EXPECT_EQ("MRXS", byIni->getDriverId());
    EXPECT_EQ(readAll(byEntry->getScene(0)), readAll(byIni->getScene(0)));
}

TEST(MRXSIntegration, autoIgnoresLetterCase)
{
    ScratchDir dir;
    // Written under the upper-case name directly: a rename that changes only the case is
    // refused by some sandboxed file systems, and is not what this test is about.
    fs::copy(dataDir() / "nomap-d1", dir.path() / "UPPER", fs::copy_options::recursive);
    const fs::path upper = dir.path() / "UPPER.MRXS";
    fs::copy_file(dataDir() / "nomap-d1.mrxs", upper);
    EXPECT_EQ("MRXS", slideio::openSlide(upper.u8string(), "")->getDriverId());
}

TEST(MRXSIntegration, autoOpensAUnicodePath)
{
    ScratchDir dir;
    const fs::path name = fs::u8path(u8"препарат");
    const fs::path entry = copyBundle("nomap-d1", dir.path(), name);
    const auto slide = slideio::openSlide(entry.u8string(), "");
    EXPECT_EQ("MRXS", slide->getDriverId());
    EXPECT_EQ(name.u8string(), slide->getScene(0)->getName());
    EXPECT_EQ(readAll(slideio::openSlide(fixture("nomap-d1.mrxs"), "")->getScene(0)),
              readAll(slide->getScene(0)));
}

TEST(MRXSIntegration, anExtractedBundleOpensThroughItsIni)
{
    // The data directory alone, as the TCIA bundles were supplied: no sibling .mrxs.
    ScratchDir dir;
    fs::copy(dataDir() / "nomap-d1", dir.path() / "extracted", fs::copy_options::recursive);
    const auto slide = slideio::openSlide((dir.path() / "extracted" / "Slidedat.ini").u8string(), "");
    EXPECT_EQ("MRXS", slide->getDriverId());
    EXPECT_EQ("extracted", slide->getScene(0)->getName());
}

TEST(MRXSIntegration, aCorruptBundleUnderAutoReportsItsOwnError)
{
    // Detection is by name, so a broken bundle reaches the MRXS driver and says what is
    // wrong, rather than falling through to GDAL or to "Cannot find driver".
    const std::string message = openError(fixture("bad-slide-id.mrxs"));
    EXPECT_NE(std::string::npos, message.find("MRXS corrupt:")) << message;
}

TEST(MRXSIntegration, theFiveChannelLimitReachesThePublicApi)
{
    const std::string message = openError(fixture("two-image-groups.mrxs"));
    EXPECT_NE(std::string::npos, message.find("MRXS unsupported:")) << message;
}

TEST(MRXSIntegration, aSceneReopensByItsPathAndDriver)
{
    // What the converter does for its reader threads: reopen the scene by file path and
    // driver id. Falling back to sharing the scene is silent at the default log level, so
    // the reopen is checked here directly.
    const auto slide = slideio::openSlide(fixture("rawmap-d2.mrxs"), "");
    const auto scene = slide->getScene(0);
    const auto reopened = slideio::openSlide(scene->getFilePath(), slide->getDriverId())->getScene(0);
    EXPECT_EQ(scene->getRect(), reopened->getRect());
    EXPECT_EQ(readAll(scene), readAll(reopened));
}

TEST(MRXSIntegration, anMrxsSceneConvertsToSvs)
{
    const auto scene = slideio::openSlide(fixture("rawmap-d2.mrxs"), "")->getScene(0);
    ScratchDir dir;
    const std::string output = (dir.path() / "converted.svs").u8string();
    slideio::converter::SVSJpegConverterParameters parameters;
    parameters.setQuality(99);
    parameters.setNumEncodingThreads(1);
    parameters.setNumReadingThreads(2);     // the reader threads reopen the scene by path and driver
    slideio::converter::convertScene(scene, parameters, output, 1);
    const auto converted = slideio::openSlide(output, "SVS")->getScene(0);
    ASSERT_EQ(scene->getRect(), converted->getRect());
    const std::vector<uint8_t> a = readAll(scene), b = readAll(converted);
    ASSERT_EQ(a.size(), b.size());
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += std::abs(int(a[i]) - int(b[i]));
    }
    EXPECT_LT(sum / a.size(), 2.0);     // JPEG at quality 99
}
