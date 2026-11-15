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
#include <optional>
#include <string>
#include <vector>

namespace slideio::mrxs
{
    /**@brief the content box a cropped scene presents (open options design, section 7.1).
     *
     * level0 is the level-0 content bounds rounded outward and clipped to the canvas;
     * levels[l] is that box at level l, origin floored and far edge ceiled, clipped to the
     * level. Both are in nominal (uncropped) pixels of their level.*/
    struct CropBox
    {
        cv::Rect level0;
        std::vector<cv::Rect> levels;
    };

    /**@brief the crop box, or nothing when there are no drawables or they miss the canvas.*/
    SLIDEIO_MRXS_EXPORTS std::optional<CropBox> computeCropBox(const Geometry& geometry);

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
        bool cropRequested = false;     ///< mrxs.crop_to_content as opened
        std::optional<CropBox> crop;    ///< set when requested and a box exists

        /// cropToContent defaults to false here: the dataset's own tests describe the
        /// nominal geometry. The driver applies the option's real default (true).
        static std::shared_ptr<const MRXSDataset> open(const std::string& entryPath,
                                                       const RuntimeOptions& options = RuntimeOptions(),
                                                       bool cropToContent = false);
    };
}
