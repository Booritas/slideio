// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsdecoder.hpp"
#include <opencv2/core.hpp>
#include <cstdint>

namespace slideio::mrxs
{
    /**@brief composites a level's drawables into RGB (design section 6).
     *
     * Each drawable is cut from its decoded image through a transparent
     * ceil(tileWidth) x ceil(tileHeight) intermediate at its fractional source offset,
     * placed at its fractional destination with bilinear sampling (transparent outside),
     * and composited in descending grid order with the saturating operator into a
     * premultiplied float accumulator; the slide's background goes underneath last.
     * The output depends only on the level and the integer rectangle.*/
    class SLIDEIO_MRXS_EXPORTS Renderer
    {
    public:
        /// Pixels of float RGBA accumulator per band: a band is 16 bytes a pixel.
        static constexpr int64_t kBandPixels = int64_t(4) << 20;

        explicit Renderer(const MRXSDataset& dataset) : m_dataset(dataset) {}

        /// Renders rect (level pixels, inside the level) into out as CV_8UC3 RGB.
        /// The band size bounds memory and never changes a pixel.
        void render(int level, const cv::Rect& rect, cv::Mat& out, int64_t bandPixels = kBandPixels) const;

    private:
        const MRXSDataset& m_dataset;
    };
}
