// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once

#include "slideio/processor/slideio_processor_def.hpp"
#include <opencv2/core.hpp>
#include <memory>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio {
    class Project;

    class SLIDEIO_PROCESSOR_EXPORTS MultiResolutionSegmentationParameters {
    private:
        double m_scale = 1.;
        cv::Size m_tileSize = {512, 512};
        cv::Size m_tileOverlapping = {5, 5};

    public:
        MultiResolutionSegmentationParameters(double scale, cv::Size tileSize) : m_scale(scale), m_tileSize(tileSize) {}

        double getScale() const {
            return m_scale;
        }

        cv::Size getTileSize() const {
            return m_tileSize;
        }

        cv::Size getTileOverlapping() const {
            return m_tileOverlapping;
        }

        void setTileOverlapping(cv::Size tileOverlapping) {
            m_tileOverlapping = tileOverlapping;
        }
    };

    void SLIDEIO_PROCESSOR_EXPORTS mutliResolutionSegmentation(std::shared_ptr <Project> &project,
                                                               std::shared_ptr <MultiResolutionSegmentationParameters> &params);
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif