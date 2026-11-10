// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The stitched read path (design §5.5 and §6), against the corpus only. The seams are
// checked against the storage tiles they come from, decoded here independently of the
// driver and placed by the geometry's positions and cuts.
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/drivers/bif/bifgeometry.hpp"
#include "slideio/drivers/bif/bifimagedriver.hpp"
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;
using namespace slideio::bif;

namespace
{
    // The level-0 directory of a slide and the geometry of its scanned AOIs, in scene
    // order, recomputed from the stitching XML without BIFSlide.
    class SlideSource
    {
    public:
        explicit SlideSource(const std::string& path)
        {
            m_tiff.openTiffFile(path);
            std::vector<TiffDirectory> dirs;
            TiffTools::scanFile(m_tiff.getHandle(), dirs);
            for (const auto& dir : dirs) {
                const auto level = parseLevelDescription(dir.description);
                if (level && level->level == 0) {
                    m_dir = dir;
                    break;
                }
            }
            const StitchInfo stitch = parseStitchXml(TiffTools::readXmlPacket(m_tiff.getHandle(), m_dir));
            const auto medianX = slideMedianOverlap(stitch.aois, JointAxis::Horizontal);
            const auto medianY = slideMedianOverlap(stitch.aois, JointAxis::Vertical);
            for (const auto& aoi : stitch.aois) {
                if (aoi.scanned) {
                    m_aois.push_back(aoi);
                    m_geometries.emplace_back(aoi, estimateOverlaps(aoi, medianX, medianY));
                }
            }
        }
        const Aoi& aoi(int scene) const { return m_aois.at(scene); }
        const AoiGeometry& geometry(int scene) const { return m_geometries.at(scene); }

        // All channels of the level-0 storage tile holding AOI tile (col,row).
        cv::Mat tile(int scene, int col, int row)
        {
            const Aoi& a = aoi(scene);
            EXPECT_EQ(0, a.originX % m_dir.tileWidth);
            EXPECT_EQ(0, a.originY % m_dir.tileHeight);
            EXPECT_EQ(m_dir.tileWidth, a.tileWidth);
            EXPECT_EQ(m_dir.tileHeight, a.tileHeight);
            const int across = (m_dir.width + m_dir.tileWidth - 1) / m_dir.tileWidth;
            const int index = (a.originY / a.tileHeight + row) * across + a.originX / a.tileWidth + col;
            cv::Mat raster;
            m_tiff.readTile(m_dir, index, {}, raster);
            return raster;
        }

        // The level-0 output rect, which must lie in at most two AOI tiles, assembled
        // from the storage tiles: output pixel X of AOI column c is pixel X - x[c] of its
        // tile, and column c owns the output range [cut[c-1], cut[c]).
        cv::Mat expected(int scene, const cv::Rect& rect)
        {
            const AoiGeometry& g = geometry(scene);
            cv::Mat out(rect.size(), CV_8UC3, cv::Scalar::all(0));
            const auto segments = [](int begin, int end, const std::vector<int>& cuts) {
                // [first, last) of the output range, and the AOI tile owning it
                std::vector<std::array<int, 3>> result;
                int from = begin;
                while (from < end) {
                    const int index = static_cast<int>(
                        std::upper_bound(cuts.begin(), cuts.end(), from) - cuts.begin());
                    const int to = index < static_cast<int>(cuts.size()) ? std::min(end, cuts[index]) : end;
                    result.push_back({ from, to, index });
                    from = to;
                }
                return result;
            };
            const auto columns = segments(rect.x, rect.br().x, g.columnCuts());
            const auto rows = segments(rect.y, rect.br().y, g.rowCuts());
            EXPECT_LE(columns.size() * rows.size(), 2u);
            for (const auto& row : rows) {
                for (const auto& column : columns) {
                    const cv::Mat source = tile(scene, column[2], row[2]);
                    const int sx = column[0] - g.columnPositions()[column[2]];
                    const int sy = row[0] - g.rowPositions()[row[2]];
                    const cv::Size size(column[1] - column[0], row[1] - row[0]);
                    source(cv::Rect(cv::Point(sx, sy), size))
                        .copyTo(out(cv::Rect(cv::Point(column[0] - rect.x, row[0] - rect.y), size)));
                }
            }
            return out;
        }

    private:
        TIFFKeeper m_tiff;
        TiffDirectory m_dir;
        std::vector<Aoi> m_aois;
        std::vector<AoiGeometry> m_geometries;
    };

    std::shared_ptr<CVScene> openScene(const std::string& path, int scene)
    {
        BIFImageDriver driver;
        auto slide = driver.openFile(path);
        return slide->getScene(scene);
    }

    cv::Mat readLevel(const std::shared_ptr<CVScene>& scene, int level, const cv::Rect& rect,
                      const cv::Size& size, const std::vector<int>& channels = {})
    {
        cv::Mat out;
        scene->readResampledLevelBlockChannels(level, rect, size, channels, out);
        return out;
    }

    bool identical(const cv::Mat& a, const cv::Mat& b)
    {
        return a.size() == b.size() && a.type() == b.type() && cv::norm(a, b, cv::NORM_INF) == 0;
    }

    // The middle of the range a tile owns, so that a strip placed there stays in it.
    int middleOf(const std::vector<int>& cuts, int index, int extent)
    {
        const int begin = index == 0 ? 0 : cuts[index - 1];
        const int end = index < static_cast<int>(cuts.size()) ? cuts[index] : extent;
        return (begin + end) / 2;
    }

    // A 64 px strip centred on column cut `boundary` (or row cut, if `rows`), 256 px
    // long, in the middle of the tile range it crosses.
    cv::Rect seamStrip(const AoiGeometry& g, int boundary, bool rows)
    {
        if (rows) {
            const int cut = g.rowCuts()[boundary];
            const int x = middleOf(g.columnCuts(), static_cast<int>(g.columnCuts().size()) / 2, g.size().width);
            return { x - 128, cut - 32, 256, 64 };
        }
        const int cut = g.columnCuts()[boundary];
        const int y = middleOf(g.rowCuts(), static_cast<int>(g.rowCuts().size()) / 2, g.size().height);
        return { cut - 32, y - 128, 64, 256 };
    }

    double median(std::vector<double> values)
    {
        std::sort(values.begin(), values.end());
        const size_t n = values.size();
        return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.;
    }

    // Mean absolute difference, over all channels, of two equally sized lines.
    double lineDifference(const cv::Mat& a, const cv::Mat& b)
    {
        cv::Mat diff;
        cv::absdiff(a, b, diff);
        const cv::Scalar mean = cv::mean(diff);
        return (mean[0] + mean[1] + mean[2]) / 3.;
    }

    // The range [begin, end) of output lines that tile `index` owns along one axis.
    std::pair<int, int> ownedRange(const std::vector<int>& cuts, int index, int extent)
    {
        return { index == 0 ? 0 : cuts[index - 1],
                 index < static_cast<int>(cuts.size()) ? cuts[index] : extent };
    }

    // What boundary `b` looks like with its overlap ignored: the last storage column of
    // tile A beside the first one of tile B (rows, for a row boundary), over the lines
    // each tile owns across the seam -- the lines the stitched seam covers.
    double unstitchedDifference(SlideSource& source, int scene, int b, bool rows)
    {
        const AoiGeometry& g = source.geometry(scene);
        const Aoi& aoi = source.aoi(scene);
        const auto& cuts = rows ? g.columnCuts() : g.rowCuts();
        const auto& positions = rows ? g.columnPositions() : g.rowPositions();
        const int extent = rows ? g.size().width : g.size().height;
        const int across = rows ? aoi.cols : aoi.rows;
        double sum = 0;
        int count = 0;
        for (int i = 0; i < across; ++i) {
            const auto [begin, end] = ownedRange(cuts, i, extent);
            const cv::Range local(begin - positions[i], end - positions[i]);
            if (rows) {
                const cv::Mat a = source.tile(scene, i, b), c = source.tile(scene, i, b + 1);
                sum += lineDifference(a.row(aoi.tileHeight - 1).colRange(local), c.row(0).colRange(local))
                    * local.size();
            }
            else {
                const cv::Mat a = source.tile(scene, b, i), c = source.tile(scene, b + 1, i);
                sum += lineDifference(a.col(aoi.tileWidth - 1).rowRange(local), c.col(0).rowRange(local))
                    * local.size();
            }
            count += local.size();
        }
        return sum / count;
    }

    // Measured on OS-1 (mean absolute difference across the seam / unstitched / ratio):
    //   column boundary 0:  5.14 / 9.65 / 0.53   (much of it glass, so both are low)
    //   column boundary 57: 8.95 / 24.30 / 0.37
    //   row boundary 37:    8.90 / 17.95 / 0.50
    // A cut that ignored the overlap (cut at the tile edge, duplicating ~110 px of
    // tissue) reads exactly the unstitched pair, ratio 1.0, so it fails; the worst seam
    // above passes with 0.22 to spare.
    constexpr double MaxSeamRatio = 0.75;

    // `strip` holds the 22 lines cut-11 .. cut+10 across the seam, so the seam pair is
    // (10, 11). The 10 pairs either side are only reported: the two cameras disagree
    // photometrically on the same tissue, so the seam pair sits at about 1.5-2x them
    // even when placement is right (design §5.4), and that is for the manual sign-off.
    void checkNoDuplication(const cv::Mat& strip, double unstitched, const std::string& label)
    {
        ASSERT_EQ(22, strip.cols);
        std::vector<double> others;
        for (int i = 0; i < 21; ++i) {
            if (i != 10) {
                others.push_back(lineDifference(strip.col(i), strip.col(i + 1)));
            }
        }
        const double seam = lineDifference(strip.col(10), strip.col(11));
        std::cout << label << ": seam " << seam << ", unstitched " << unstitched << ", ratio "
                  << seam / unstitched << ", median of neighbours " << median(others) << std::endl;
        EXPECT_LT(seam, MaxSeamRatio * unstitched) << label;
    }

    const char* const Folder = "bif/openslide";
}

TEST(BIFCorpusRead, SeamMatchesSources)
{
    const std::string os1 = TestTools::getTestImagePath(Folder, "OS-1.bif");
    const std::string ventana = TestTools::getTestImagePath(Folder, "Ventana-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(os1);
    SLIDEIO_SKIP_IF_IMAGE_MISSING(ventana);
    {
        SlideSource source(ventana);
        const AoiGeometry& g = source.geometry(0);
        ASSERT_GE(g.columnCuts().size(), 4u);
        // boundary 3 overlaps by 24 px, so it is cut 12 px into column 4
        EXPECT_EQ(g.columnPositions()[4] + 12, g.columnCuts()[3]);
        auto scene = openScene(ventana, 0);
        const cv::Rect strip = seamStrip(g, 3, false);
        const cv::Mat actual = readLevel(scene, 0, strip, strip.size());
        EXPECT_TRUE(identical(source.expected(0, strip), actual)) << "Ventana-1 column boundary 3";
    }
    SlideSource source(os1);
    const AoiGeometry& g = source.geometry(0);
    auto scene = openScene(os1, 0);
    const int columns = static_cast<int>(g.columnCuts().size());
    const int rows = static_cast<int>(g.rowCuts().size());
    ASSERT_GE(columns, 2);
    ASSERT_GE(rows, 1);
    for (const auto& [boundary, isRow] : std::vector<std::pair<int, bool>>{
             { 0, false }, { columns / 2, false }, { rows / 2, true } }) {
        const cv::Rect strip = seamStrip(g, boundary, isRow);
        const cv::Mat actual = readLevel(scene, 0, strip, strip.size());
        EXPECT_TRUE(identical(source.expected(0, strip), actual))
            << "OS-1 " << (isRow ? "row" : "column") << " boundary " << boundary;
    }
}

TEST(BIFCorpusRead, SeamHasNoDuplication)
{
    const std::string path = TestTools::getTestImagePath(Folder, "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    SlideSource source(path);
    const AoiGeometry& g = source.geometry(0);
    auto scene = openScene(path, 0);
    const cv::Size size = g.size();
    const int columns = static_cast<int>(g.columnCuts().size());
    const int rows = static_cast<int>(g.rowCuts().size());
    for (int boundary : { 0, columns / 2 }) {
        const int cut = g.columnCuts()[boundary];
        const cv::Mat strip = readLevel(scene, 0, { cut - 11, 0, 22, size.height }, { 22, size.height });
        checkNoDuplication(strip, unstitchedDifference(source, 0, boundary, false), "OS-1 column boundary " + std::to_string(boundary));
    }
    const int cut = g.rowCuts()[rows / 2];
    const cv::Mat strip = readLevel(scene, 0, { 0, cut - 11, size.width, 22 }, { size.width, 22 });
    checkNoDuplication(strip.t(), unstitchedDifference(source, 0, rows / 2, true), "OS-1 row boundary " + std::to_string(rows / 2));
}

TEST(BIFCorpusRead, LevelEqualsScaled)
{
    const std::string path = TestTools::getTestImagePath(Folder, "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    SlideSource source(path);
    const AoiGeometry& g = source.geometry(0);
    auto scene = openScene(path, 0);
    // across a column seam and a row seam at level 2
    const cv::Rect r(g.columnCuts()[0] / 4 - 200, g.rowCuts()[0] / 4 - 150, 400, 300);
    const cv::Mat level = readLevel(scene, 2, r, r.size());
    cv::Mat scaled;
    const cv::Rect r0(r.x * 4, r.y * 4, r.width * 4, r.height * 4);
    scene->readResampledBlockChannels(r0, r.size(), {}, scaled);
    EXPECT_TRUE(identical(level, scaled));
}

TEST(BIFCorpusRead, ScaledEqualsResize)
{
    const std::string path = TestTools::getTestImagePath(Folder, "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    SlideSource source(path);
    const AoiGeometry& g = source.geometry(0);
    auto scene = openScene(path, 0);
    const cv::Rect rect(g.columnCuts()[0] - 450, g.rowCuts()[0] - 450, 900, 900);
    const cv::Mat native = readLevel(scene, 0, rect, rect.size());
    const cv::Mat scaled = readLevel(scene, 0, rect, rect.size() / 3);
    cv::Mat expected;
    cv::resize(native, expected, rect.size() / 3, 0, 0, cv::INTER_AREA);
    ASSERT_EQ(expected.size(), scaled.size());
    ASSERT_EQ(expected.type(), scaled.type());
    EXPECT_LE(cv::norm(expected, scaled, cv::NORM_INF), 1.);
}

TEST(BIFCorpusRead, ChannelOrder)
{
    const std::string path = TestTools::getTestImagePath(Folder, "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    SlideSource source(path);
    const AoiGeometry& g = source.geometry(0);
    auto scene = openScene(path, 0);
    const cv::Rect rect(g.columnCuts()[0] - 100, g.rowCuts()[0] - 100, 200, 200);
    const cv::Mat full = readLevel(scene, 0, rect, rect.size());
    const cv::Mat subset = readLevel(scene, 0, rect, rect.size(), { 2, 0 });
    ASSERT_EQ(CV_8UC2, subset.type());
    std::vector<cv::Mat> planes, subsetPlanes;
    cv::split(full, planes);
    cv::split(subset, subsetPlanes);
    EXPECT_TRUE(identical(planes[2], subsetPlanes[0]));
    EXPECT_TRUE(identical(planes[0], subsetPlanes[1]));
}

TEST(BIFCorpusRead, CoarseEdge)
{
    const std::string path = TestTools::getTestImagePath(Folder, "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto scene = openScene(path, 0);
    ASSERT_EQ(10, scene->getNumZoomLevels());
    const cv::Size size = scene->getZoomLevelInfo(9)->getSize();
    ASSERT_GT(size.width, 0);
    ASSERT_GT(size.height, 0);
    for (const cv::Rect& rect : { cv::Rect(cv::Point(0, 0), size),
                                  cv::Rect(0, size.height - 1, size.width, 1),
                                  cv::Rect(size.width - 1, 0, 1, size.height) }) {
        const cv::Mat block = readLevel(scene, 9, rect, rect.size());
        EXPECT_EQ(rect.size(), block.size()) << rect;
        EXPECT_EQ(CV_8UC3, block.type()) << rect;
    }
}

TEST(BIFCorpusRead, PastEdgeBackground)
{
    const std::string path = TestTools::getTestImagePath(Folder, "OS-1.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    auto scene = openScene(path, 0);
    const cv::Rect aoi = scene->getRect();
    for (const cv::Rect& rect : { cv::Rect(aoi.width - 200, aoi.height - 200, 300, 300),
                                  cv::Rect(-100, -100, 300, 300) }) {
        const cv::Mat block = readLevel(scene, 0, rect, rect.size());
        ASSERT_EQ(rect.size(), block.size());
        const cv::Rect inside = (rect & aoi) - rect.tl();
        const cv::Mat inner = readLevel(scene, 0, rect & aoi, (rect & aoi).size());
        EXPECT_TRUE(identical(inner, block(inside))) << rect;
        for (int y = 0; y < block.rows; ++y) {
            for (int x = 0; x < block.cols; ++x) {
                if (!inside.contains(cv::Point(x, y))) {
                    ASSERT_EQ(cv::Vec3b(255, 255, 255), block.at<cv::Vec3b>(y, x))
                        << rect << " at " << x << "," << y;
                }
            }
        }
    }
}

TEST(BIFCorpusRead, Concurrency)
{
    for (const char* file : { "OS-2.bif", "Ventana-1.bif" }) {
        const std::string path = TestTools::getTestImagePath(Folder, file);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        SCOPED_TRACE(file);
        BIFImageDriver driver;
        TestTools::concurrentReadIdentityTestAllScenes(path, driver);
    }
}
