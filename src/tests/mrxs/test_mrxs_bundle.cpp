// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// MRXS entry points: which paths the driver claims, and how an entry path resolves
// to the bundle's data directory (design section 2). Reads only the committed
// fixtures in src/tests/mrxs/data, so it runs without the image corpus.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/drivers/mrxs/mrxsbundle.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tempfile.hpp"

namespace fs = std::filesystem;
using slideio::mrxs::BundlePaths;
using slideio::mrxs::resolveBundle;

namespace
{
    std::string fixture(const std::string& relative)
    {
        return (fs::path(MRXS_TEST_DATA_DIR) / fs::path(relative)).u8string();
    }

    // A scratch directory removed when the test ends.
    class ScratchDir
    {
    public:
        ScratchDir() : m_path(fs::temp_directory_path() / slideio::TempFile::unique_path("mrxs-%%%%-%%%%")) {
            fs::create_directories(m_path);
        }
        ~ScratchDir() { std::error_code ec; fs::remove_all(m_path, ec); }
        const fs::path& path() const { return m_path; }
        void touch(const fs::path& relative) const {
            fs::create_directories((m_path / relative).parent_path());
            std::ofstream(m_path / relative).put('x');
        }
    private:
        fs::path m_path;
    };
}

TEST(MRXSDriver, idIsMRXS)
{
    EXPECT_EQ("MRXS", slideio::MRXSImageDriver().getID());
}

TEST(MRXSDriver, claimsMrxsFilesInAnyCase)
{
    slideio::MRXSImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("D:/slides/CMU-1.mrxs"));
    EXPECT_TRUE(driver.canOpenFile("D:/slides/CMU-1.MRXS"));
    EXPECT_TRUE(driver.canOpenFile("/data/slides/a.b.Mrxs"));
}

TEST(MRXSDriver, claimsSlidedatIniInAnyCase)
{
    slideio::MRXSImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("D:/slides/CMU-1/Slidedat.ini"));
    EXPECT_TRUE(driver.canOpenFile("/data/CMU-1/SLIDEDAT.INI"));
}

TEST(MRXSDriver, doesNotClaimOtherFiles)
{
    slideio::MRXSImageDriver driver;
    EXPECT_FALSE(driver.canOpenFile("D:/slides/settings.ini"));
    EXPECT_FALSE(driver.canOpenFile("D:/slides/MySlidedat.ini"));
    EXPECT_FALSE(driver.canOpenFile("D:/slides/Slidedat.ini.bak"));
    EXPECT_FALSE(driver.canOpenFile("D:/slides/CMU-1.mrxs.bak"));
    EXPECT_FALSE(driver.canOpenFile("D:/slides/CMU-1.svs"));
    EXPECT_FALSE(driver.canOpenFile("D:/slides/mrxs"));
}

TEST(MRXSDriver, decidesByNameWithoutTouchingTheDisk)
{
    // AUTO detection must not probe: a corrupt or absent bundle is reported by openFile,
    // not silently handed to the next driver.
    slideio::MRXSImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("Z:/does/not/exist.mrxs"));
}

TEST(MRXSBundle, resolvesAnEntryFileToItsSiblingDirectory)
{
    const BundlePaths paths = resolveBundle(fixture("nomap-d1.mrxs"));
    EXPECT_EQ(fs::path(fixture("nomap-d1")), paths.dataDir);
    EXPECT_EQ(fs::path(fixture("nomap-d1/Slidedat.ini")), paths.ini);
    EXPECT_EQ("nomap-d1", paths.sceneName);
}

TEST(MRXSBundle, resolvesSlidedatIniToItsParentDirectory)
{
    const BundlePaths paths = resolveBundle(fixture("nomap-d1/Slidedat.ini"));
    EXPECT_EQ(fs::path(fixture("nomap-d1")), paths.dataDir);
    EXPECT_EQ(fs::path(fixture("nomap-d1/Slidedat.ini")), paths.ini);
    EXPECT_EQ("nomap-d1", paths.sceneName);
}

TEST(MRXSBundle, anIniEntryNeedsNoMrxsFile)
{
    // An extracted bundle: the data directory alone, no entry file next to it.
    ScratchDir dir;
    dir.touch("extracted/Slidedat.ini");
    const BundlePaths paths = resolveBundle((dir.path() / "extracted" / "Slidedat.ini").u8string());
    EXPECT_EQ(dir.path() / "extracted", paths.dataDir);
    EXPECT_EQ("extracted", paths.sceneName);
}

TEST(MRXSBundle, anUpperCaseEntryResolvesToTheSameDirectory)
{
    ScratchDir dir;
    dir.touch("Slide.MRXS");
    dir.touch("Slide/Slidedat.ini");
    const BundlePaths paths = resolveBundle((dir.path() / "Slide.MRXS").u8string());
    EXPECT_EQ(dir.path() / "Slide", paths.dataDir);
    EXPECT_EQ("Slide", paths.sceneName);
}

TEST(MRXSBundle, rejectsAMissingEntryFile)
{
    ScratchDir dir;
    dir.touch("Slide/Slidedat.ini");
    EXPECT_THROW(resolveBundle((dir.path() / "Slide.mrxs").u8string()), slideio::RuntimeError);
}

TEST(MRXSBundle, rejectsAnEntryWithoutItsDataDirectory)
{
    ScratchDir dir;
    dir.touch("Slide.mrxs");
    EXPECT_THROW(resolveBundle((dir.path() / "Slide.mrxs").u8string()), slideio::RuntimeError);
}

TEST(MRXSBundle, rejectsADataDirectoryWithoutSlidedatIni)
{
    ScratchDir dir;
    dir.touch("Slide.mrxs");
    dir.touch("Slide/Index.dat");
    EXPECT_THROW(resolveBundle((dir.path() / "Slide.mrxs").u8string()), slideio::RuntimeError);
}

TEST(MRXSBundle, rejectsAPathThatIsNoEntry)
{
    ScratchDir dir;
    dir.touch("notes.txt");
    EXPECT_THROW(resolveBundle((dir.path() / "notes.txt").u8string()), slideio::RuntimeError);
}

TEST(MRXSBundle, resolvesAUnicodePath)
{
    ScratchDir dir;
    const fs::path name = fs::u8path(u8"\u0442\u0435\u0441\u0442");
    const fs::path entry = fs::u8path(name.u8string() + ".mrxs");
    dir.touch(entry);
    dir.touch(name / "Slidedat.ini");
    const BundlePaths paths = resolveBundle((dir.path() / entry).u8string());
    EXPECT_EQ(dir.path() / name, paths.dataDir);
    EXPECT_EQ(name.u8string(), paths.sceneName);
}
