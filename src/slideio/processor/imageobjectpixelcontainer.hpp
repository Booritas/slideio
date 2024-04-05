// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <opencv2/core/mat.hpp>

#include "slideio/processor/imageobject.hpp"
#include "slideio/processor/imageobjectpixeliterator.hpp"
#include "slideio/processor/imageobjectborderiterator.hpp"

namespace slideio
{
    class ImageObjectPixelContainer
    {
    public:
        ImageObjectPixelContainer(ImageObject* object, cv::Mat& tile, cv::Rect tileRect) :
            m_object(object), m_tile(tile), m_tileRect(tileRect) {
        }

        ImageObjectPixelIterator begin() {
            return ImageObjectPixelIterator(m_object, m_tile, m_tileRect, true);
        }

        ImageObjectPixelIterator end() {
            return ImageObjectPixelIterator(m_object, m_tile, m_tileRect, false);
        }
    private:
        ImageObject* m_object;
        cv::Mat m_tile;
        cv::Rect m_tileRect;
    };
}
