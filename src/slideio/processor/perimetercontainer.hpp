// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <opencv2/core/mat.hpp>

#include "slideio/processor/imageobject.hpp"
#include "slideio/processor/perimeteriterator.hpp"

namespace slideio
{
    class PerimeterContainer
    {
    public:
        PerimeterContainer(ImageObject* object, cv::Mat& tile, const cv::Point& tileOrg) :
            m_object(object), m_tile(tile), m_tileOrg(tileOrg) {
        }

        PerimeterIterator begin() {
            return PerimeterIterator(m_object, m_tile, m_tileOrg, true);
        }
        PerimeterIterator end() {
            return PerimeterIterator(m_object, m_tile, m_tileOrg, false);
        }
    private:
        ImageObject* m_object;
        cv::Mat m_tile;
        cv::Point m_tileOrg;
    };
}
