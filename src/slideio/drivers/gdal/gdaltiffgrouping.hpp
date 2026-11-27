// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <vector>

namespace slideio
{
    /**@brief One scene of a tiff file: a base directory and its zoom levels.*/
    struct GDALTiffSceneGroup
    {
        /**@brief Index of the top-level directory the scene is ordered by.*/
        int firstDirectory;
        /**@brief levels[0] is the base; widths descend.*/
        std::vector<TiffDirectory> levels;
    };

    /**@brief Groups the top-level directories of a tiff file into scenes with
     * zoom levels. SubIFDs are read from topLevel[i].subdirectories.
     * The result is sorted by firstDirectory.*/
    SLIDEIO_GDAL_EXPORTS std::vector<GDALTiffSceneGroup>
        groupTiffDirectories(const std::vector<TiffDirectory>& topLevel);
}
