// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "slideio/drivers/mrxs/mrxsindex.hpp"
#include "slideio/drivers/mrxs/mrxsruntime.hpp"
#include <opencv2/core.hpp>
#include <memory>
#include <string>

namespace slideio::mrxs
{
    /**@brief everything an open bundle needs to be read, validated and immutable.
     *
     * Scenes share it by shared_ptr<const MRXSDataset>, so they outlive the slide that
     * created them (design section 3). Its one mutable part, the caches, sits behind a
     * shared_ptr: constness of the dataset is shallow over that pointer, and the runtime
     * synchronises itself.*/
    struct SLIDEIO_MRXS_EXPORTS MRXSDataset
    {
        SlideStructure structure;
        Geometry geometry;
        cv::Vec3b background;       ///< RGB, from level 0's IMAGE_FILL_COLOR_BGR
        std::shared_ptr<MRXSRuntime> runtime;

        static std::shared_ptr<const MRXSDataset> open(const std::string& entryPath,
                                                       const RuntimeOptions& options = RuntimeOptions());
    };
}
