// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "slideio/drivers/gdal/gdaltiffgrouping.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace slideio;

namespace
{
    constexpr int kReducedResolution = 1;
    constexpr int kMask = 4;

    bool isMask(const TiffDirectory& d) { return (d.subFileType & kMask) != 0; }
    bool isReduced(const TiffDirectory& d) { return (d.subFileType & kReducedResolution) != 0; }

    bool keepsAspectRatio(const TiffDirectory& base, const TiffDirectory& c)
    {
        // Cast before multiplying: for a whole-slide base the product of two
        // dimensions is past int32 (100000x80000 with a 50000-wide level is 4e9).
        const double baseW = static_cast<double>(base.width);
        const double baseH = static_cast<double>(base.height);
        const double expectedH = std::round(baseH * c.width / baseW);
        const double expectedW = std::round(baseW * c.height / baseH);
        // A writer rounds each dimension of a level by up to one pixel, and one
        // pixel of the height predicts baseW/baseH pixels of the width (and the
        // other way round), so the tolerance on each side scales with the ratio.
        // A fixed 1 rejects a small level of a long thin base: 98x33 of
        // 100001x33000 predicts a width of 100.
        const double toleranceW = std::max(1.0, std::ceil(baseW / baseH));
        const double toleranceH = std::max(1.0, std::ceil(baseH / baseW));
        return std::abs(c.height - expectedH) <= toleranceH && std::abs(c.width - expectedW) <= toleranceW;
    }

    // Rule 4.
    bool isLevelOf(const TiffDirectory& base, const TiffDirectory& c)
    {
        if (base.width <= 0 || base.height <= 0 || c.width <= 0 || c.height <= 0) {
            return false;
        }
        return !isMask(c)
            && c.channels == base.channels
            && c.dataType == base.dataType
            && c.bitsPerSample == base.bitsPerSample
            && c.width < base.width
            && c.height < base.height
            && keepsAspectRatio(base, c);
    }

    struct Pending
    {
        GDALTiffSceneGroup group;
        std::vector<TiffDirectory> candidates; // SubIFDs first, then top-level reduced IFDs
    };

    void finish(Pending& p)
    {
        std::vector<TiffDirectory> accepted;
        for (const auto& c : p.candidates) {
            const bool duplicate = std::any_of(accepted.begin(), accepted.end(),
                [&c](const TiffDirectory& a) { return a.width == c.width && a.height == c.height; });
            if (!duplicate) {
                accepted.push_back(c);
            }
        }
        std::stable_sort(accepted.begin(), accepted.end(),
            [](const TiffDirectory& a, const TiffDirectory& b) { return a.width > b.width; });
        p.group.levels.insert(p.group.levels.end(), accepted.begin(), accepted.end());
    }
}

std::vector<GDALTiffSceneGroup> slideio::groupTiffDirectories(const std::vector<TiffDirectory>& topLevel)
{
    std::vector<Pending> pending;
    int currentBase = -1; // index into pending of the open base, or -1
    // Index into pending of the most recent scene a reduced IFD started (rule 6),
    // or -1. Its first IFD is the base later reduced IFDs fall back to.
    int reducedBase = -1;

    auto startScene = [&](const TiffDirectory& d) -> int {
        Pending p;
        p.group.firstDirectory = d.dirIndex;
        p.group.levels.push_back(d);
        pending.push_back(std::move(p));
        return static_cast<int>(pending.size()) - 1;
    };

    for (const auto& dir : topLevel) {
        if (isMask(dir)) {
            continue; // rule 1
        }
        if (!isReduced(dir)) {
            currentBase = startScene(dir); // rule 2
            // Rule 6's fallback lives within one base's run only: a reduced IFD
            // after this base never reaches back across it to an earlier reduced
            // scene. A wrong merge loses data silently; a page left as its own
            // scene does not.
            reducedBase = -1;
            for (const auto& sub : dir.subdirectories) {
                if (isLevelOf(dir, sub)) {
                    pending[currentBase].candidates.push_back(sub);
                }
            }
            continue;
        }
        if (currentBase >= 0 && isLevelOf(pending[currentBase].group.levels[0], dir)) {
            pending[currentBase].candidates.push_back(dir); // rule 3
        }
        else if (reducedBase >= 0 && isLevelOf(pending[reducedBase].group.levels[0], dir)) {
            pending[reducedBase].candidates.push_back(dir); // rule 6: a level of the reduced scene
        }
        else {
            // Rule 6: a scene of its own, and the base later reduced IFDs fall back
            // to. The run of the original base goes on.
            reducedBase = startScene(dir);
        }
    }

    if (pending.empty()) {
        // rule 7: nothing but masks
        for (const auto& dir : topLevel) {
            startScene(dir);
        }
    }

    std::vector<GDALTiffSceneGroup> result;
    result.reserve(pending.size());
    for (auto& p : pending) {
        finish(p);
        result.push_back(std::move(p.group));
    }
    return result;
}
