// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <opencv2/core.hpp>
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/slideio_enums.hpp"

namespace slideio
{
    /**@brief ICC colour conversion of raster blocks.
     *
     * The only class in the project built against lcms2. The compiled transform
     * is created once in the constructor and never mutated, so apply() is const
     * and safe to call from several threads on one instance.
     *
     * Channel order is RGB, matching slideio block buffers, NOT OpenCV's BGR.*/
    class SLIDEIO_IMAGETOOLS_EXPORTS IccTransform
    {
    public:
        IccTransform(const ColorProfile& source, ColorTarget target,
                     RenderingIntent intent, bool blackPointCompensation,
                     DataType sourceType);
        ~IccTransform();
        IccTransform(const IccTransform&) = delete;
        IccTransform& operator=(const IccTransform&) = delete;

        void apply(const cv::Mat& src, cv::OutputArray dst) const;
        DataType getOutputDataType() const { return m_outputType; }

        /**@brief parses the ICC header. Returns present=false for empty or
         * unparseable bytes rather than throwing, so a corrupt profile does not
         * stop a batch read. The source field is copied from the profile, not
         * parsed: provenance is not discoverable from the bytes.*/
        static ColorProfileInfo describe(const ColorProfile& profile);

        /**@brief a synthetic sRGB profile, marked ColorProfileSource::Assumed.*/
        static ColorProfile createSRGBProfile();
    private:
        void* m_transform = nullptr;   // cmsHTRANSFORM
        DataType m_outputType = DataType::DT_Unknown;
        int m_targetChannels = 3;
    };
}
