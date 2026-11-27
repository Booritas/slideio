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
        const double expectedH = std::round(base.height * c.width / static_cast<double>(base.width));
        const double expectedW = std::round(base.width * c.height / static_cast<double>(base.height));
        return std::abs(c.height - expectedH) <= 1 && std::abs(c.width - expectedW) <= 1;
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
        else {
            startScene(dir); // rule 6: own scene, the run of the base goes on
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
