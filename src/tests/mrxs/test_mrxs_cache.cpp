// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Decoded-image cache, file-handle reuse and concurrent reads (design section 8), on the
// committed fixtures. These run in CI under ThreadSanitizer.
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <opencv2/core.hpp>
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/drivers/mrxs/mrxsrenderer.hpp"
#include "slideio/drivers/mrxs/mrxsruntime.hpp"
#include "slideio/drivers/mrxs/mrxsscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tempfile.hpp"

namespace fs = std::filesystem;
using namespace slideio::mrxs;

namespace
{
    const fs::path dataDir() { return fs::path(MRXS_TEST_DATA_DIR); }
    std::string entry(const std::string& name) { return (dataDir() / (name + ".mrxs")).u8string(); }

    cv::Rect levelRect(const MRXSDataset& dataset, int level)
    {
        const LevelGeometry& lg = dataset.geometry.levels[level];
        return cv::Rect(0, 0, static_cast<int>(lg.width), static_cast<int>(lg.height));
    }

    cv::Mat render(const MRXSDataset& dataset, int level, const cv::Rect& rect)
    {
        cv::Mat out;
        Renderer(dataset).render(level, rect, out);
        return out;
    }

    bool identical(const cv::Mat& a, const cv::Mat& b)
    {
        return a.size() == b.size() && a.type() == b.type() && cv::norm(a, b, cv::NORM_INF) == 0;
    }

    // Distinct encoded images contributing to a region.
    size_t imagesIn(const MRXSDataset& dataset, int level, const cv::Rect& rect)
    {
        std::vector<uint32_t> records;
        const LevelGeometry& lg = dataset.geometry.levels[level];
        for (uint32_t i : lg.query(rect.x, rect.y, rect.br().x, rect.br().y)) {
            records.push_back(lg.drawables()[i].record);
        }
        std::sort(records.begin(), records.end());
        return size_t(std::unique(records.begin(), records.end()) - records.begin());
    }

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
}

TEST(MRXSCache, anImageServingManySubtilesIsDecodedOncePerRequest)
{
    // saved-c0-4's coarsest level cuts each encoded image into 16 x 16 subtiles.
    const auto dataset = MRXSDataset::open(entry("saved-c0-4"));
    const int level = 2;
    const cv::Rect rect = levelRect(*dataset, level);
    ASSERT_GT(dataset->geometry.levels[level].drawables().size(), imagesIn(*dataset, level, rect));
    render(*dataset, level, rect);
    EXPECT_EQ(int64_t(imagesIn(*dataset, level, rect)), dataset->runtime->stats().decodes);
}

TEST(MRXSCache, aWarmReadDecodesNothing)
{
    const auto dataset = MRXSDataset::open(entry("rawmap-d2"));
    const cv::Rect rect = levelRect(*dataset, 0);
    const cv::Mat cold = render(*dataset, 0, rect);
    const RuntimeStats afterCold = dataset->runtime->stats();
    const cv::Mat warm = render(*dataset, 0, rect);
    const RuntimeStats afterWarm = dataset->runtime->stats();
    EXPECT_EQ(afterCold.decodes, afterWarm.decodes);
    EXPECT_GT(afterWarm.hits, afterCold.hits);
    EXPECT_TRUE(identical(cold, warm));
}

TEST(MRXSCache, theCacheStaysWithinItsByteBudget)
{
    RuntimeOptions options;
    options.decodedCacheBytes = 12 * 12 * 3 * 4;        // four rawmap-d2 images
    const auto dataset = MRXSDataset::open(entry("rawmap-d2"), options);
    for (int level = 0; level < 4; ++level) {
        render(*dataset, level, levelRect(*dataset, level));
        EXPECT_LE(dataset->runtime->stats().cachedBytes, options.decodedCacheBytes);
    }
}

TEST(MRXSCache, aZeroBudgetCachesNothingAndChangesNoPixel)
{
    const auto cached = MRXSDataset::open(entry("rawmap-d2"));
    RuntimeOptions none;
    none.decodedCacheBytes = 0;
    const auto uncached = MRXSDataset::open(entry("rawmap-d2"), none);
    for (int level = 0; level < 4; ++level) {
        const cv::Rect rect = levelRect(*cached, level);
        EXPECT_TRUE(identical(render(*cached, level, rect), render(*uncached, level, rect))) << "level " << level;
    }
    EXPECT_EQ(0u, uncached->runtime->stats().cachedBytes);
}

TEST(MRXSCache, dataFilesAreOpenedOnceAndReused)
{
    const auto dataset = MRXSDataset::open(entry("rawmap-d2"));
    render(*dataset, 0, levelRect(*dataset, 0));
    // rawmap-d2 has three data files; tiles are spread over all of them.
    EXPECT_EQ(3, dataset->runtime->stats().fileOpens);
}

TEST(MRXSCache, theHandleCacheHonoursItsCount)
{
    RuntimeOptions options;
    options.maxOpenFiles = 1;
    options.decodedCacheBytes = 0;
    const auto dataset = MRXSDataset::open(entry("rawmap-d2"), options);
    const cv::Mat image = render(*dataset, 0, levelRect(*dataset, 0));
    EXPECT_LE(dataset->runtime->stats().openFiles, 1);
    EXPECT_TRUE(identical(render(*MRXSDataset::open(entry("rawmap-d2")), 0, levelRect(*dataset, 0)), image));
}

TEST(MRXSCache, aFailedDecodeIsNotCached)
{
    const auto dataset = MRXSDataset::open(entry("codec-mismatch"));
    const cv::Rect rect = levelRect(*dataset, 0);
    for (int attempt = 0; attempt < 2; ++attempt) {
        std::string message;
        try {
            render(*dataset, 0, rect);
        }
        catch (const slideio::RuntimeError& e) {
            message = e.what();
        }
        EXPECT_EQ(0u, message.find("MRXS corrupt:")) << "attempt " << attempt << ": " << message;
    }
}

TEST(MRXSConcurrency, theSceneReportsConcurrentReads)
{
    slideio::MRXSImageDriver driver;
    EXPECT_TRUE(driver.openFile(entry("rawmap-d2"))->getScene(0)->supportsConcurrentReads());
}

TEST(MRXSConcurrency, concurrentReadsEqualSerialReads)
{
    // Identical and disjoint regions from several threads at once, with a cache small
    // enough to evict while other threads are reading.
    RuntimeOptions options;
    options.decodedCacheBytes = 12 * 12 * 3 * 2;
    options.maxOpenFiles = 2;
    const auto shared = MRXSDataset::open(entry("rawmap-d2"), options);
    const auto reference = MRXSDataset::open(entry("rawmap-d2"));
    struct Job { int level; cv::Rect rect; cv::Mat expected; };
    std::vector<Job> jobs;
    for (int level = 0; level < 4; ++level) {
        const cv::Rect whole = levelRect(*reference, level);
        for (const cv::Rect& rect : {whole, cv::Rect(0, 0, std::max(1, whole.width / 2), whole.height),
                                     cv::Rect(whole.width / 2, 0, whole.width - whole.width / 2, whole.height)}) {
            jobs.push_back({level, rect, render(*reference, level, rect)});
        }
    }
    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t] {
            for (int round = 0; round < 20; ++round) {
                const Job& job = jobs[(t * 7 + round) % jobs.size()];
                if (!identical(render(*shared, job.level, job.rect), job.expected)) {
                    ++mismatches;
                }
            }
        });
    }
    for (auto& thread : threads) thread.join();
    EXPECT_EQ(0, mismatches.load());
}

TEST(MRXSConcurrency, concurrentPublicReadsOfOneSceneAgree)
{
    slideio::MRXSImageDriver driver;
    const auto scene = driver.openFile(entry("nomap-overlap-d2"))->getScene(0);
    const cv::Rect rect = scene->getRect();
    cv::Mat expected;
    scene->readBlock(rect, expected);
    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&] {
            for (int round = 0; round < 10; ++round) {
                cv::Mat got;
                scene->readBlock(rect, got);
                if (!identical(got, expected)) ++mismatches;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    EXPECT_EQ(0, mismatches.load());
}

TEST(MRXSConcurrency, closingTheSlideReleasesItsFiles)
{
    // On Windows a handle kept open would make the bundle undeletable after the slide
    // and its scenes are gone (CLAUDE.md, concurrency contract).
    ScratchDir dir;
    fs::copy(dataDir() / "rawmap-d2", dir.path() / "rawmap-d2", fs::copy_options::recursive);
    fs::copy_file(dataDir() / "rawmap-d2.mrxs", dir.path() / "rawmap-d2.mrxs");
    {
        slideio::MRXSImageDriver driver;
        const auto slide = driver.openFile((dir.path() / "rawmap-d2.mrxs").u8string());
        cv::Mat pixels;
        slide->getScene(0)->readBlock(slide->getScene(0)->getRect(), pixels);
        slide->getAuxImage("Macro")->readBlock(slide->getAuxImage("Macro")->getRect(), pixels);
    }
    std::error_code ec;
    fs::remove_all(dir.path() / "rawmap-d2", ec);
    EXPECT_FALSE(ec) << ec.message();
    EXPECT_FALSE(fs::exists(dir.path() / "rawmap-d2"));
}
