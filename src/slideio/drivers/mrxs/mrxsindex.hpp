// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsbundle.hpp"
#include "slideio/drivers/mrxs/mrxsini.hpp"
#include "slideio/drivers/mrxs/mrxsmetadata.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace slideio::mrxs
{
    /**@brief a byte range in one of the bundle's data files, validated against its size.*/
    struct PayloadRef
    {
        int fileNumber = -1;
        int64_t offset = 0;
        int64_t length = 0;
    };

    /**@brief one hierarchical record of a pyramid level.*/
    struct ImageRecord
    {
        int64_t imageIndex = 0;     ///< y * Nx + x in the original image lattice
        int x = 0;
        int y = 0;
        PayloadRef payload;
    };

    enum class PositionSource { None, Raw, Zlib };

    /**@brief resource budgets for index traversal (design section 4).*/
    struct IndexBudget
    {
        int64_t maxPages = int64_t(1) << 20;
        int64_t maxRecords = int64_t(1) << 28;
    };

    /**@brief a bundle's metadata and index, read and validated without decoding any pixels.*/
    struct SlideStructure
    {
        BundlePaths paths;
        IniFile ini;
        SlideMetadata metadata;
        std::vector<std::vector<ImageRecord>> levels;   ///< per stored level, in index order
        PositionSource positionSource = PositionSource::None;
        PayloadRef positions;                           ///< valid when positionSource != None
        std::map<std::string, PayloadRef> associated;   ///< "Macro", "Label", "Thumbnail" when present
    };

    /**@brief resolves, reads and validates a bundle's metadata and index.*/
    SLIDEIO_MRXS_EXPORTS SlideStructure readSlideStructure(const std::string& entryPath,
                                                           const IndexBudget& budget = IndexBudget());
}
