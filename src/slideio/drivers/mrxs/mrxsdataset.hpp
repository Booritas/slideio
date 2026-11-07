// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "slideio/drivers/mrxs/mrxsindex.hpp"
#include <opencv2/core.hpp>
#include <memory>
#include <string>

namespace slideio::mrxs
{
    /**@brief everything an open bundle needs to be read, validated and immutable.
     *
     * Scenes share it by shared_ptr<const MRXSDataset>, so they outlive the slide that
     * created them (design section 3).*/
    struct SLIDEIO_MRXS_EXPORTS MRXSDataset
    {
        SlideStructure structure;
        Geometry geometry;
        cv::Vec3b background;       ///< RGB, from level 0's IMAGE_FILL_COLOR_BGR

        static std::shared_ptr<const MRXSDataset> open(const std::string& entryPath);
    };
}
