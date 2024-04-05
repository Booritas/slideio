// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <opencv2/core/mat.hpp>

#include "slideio/processor/imageobject.hpp"

namespace slideio
{
    class ImageObjectPixelIterator
    {
    private:
        cv::Point m_current;
        ImageObject* m_object;
        cv::Rect m_rect;
        cv::Mat m_tile;
        bool m_end;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = cv::Point;
        using difference_type = std::ptrdiff_t;
        using pointer = cv::Point*;
        using reference = cv::Point&;
        ImageObjectPixelIterator(ImageObject* object, cv::Mat& tile, cv::Rect tileRect, bool begin) :
            m_object(object), m_tile(tile), m_rect(tileRect), m_end(!begin) {
            m_current = m_rect.tl();
            if (begin) {
                m_end = !moveToValid();
            }
        }

        const cv::Point& operator*() const {
            return m_current;
        }

        ImageObjectPixelIterator& operator++() {
            next();
            m_end = !moveToValid();
            return *this;
        }

        // Post-increment operator
        ImageObjectPixelIterator operator++(int) {
            ImageObjectPixelIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        // Equality operator
        bool operator==(const ImageObjectPixelIterator& other) const {
            if (m_end && other.m_end) {
                return true;
            }
            if (!m_end && !other.m_end) {
                return m_current == other.m_current;
            }
            return false;
        }

        // Inequality operator
        bool operator!=(const ImageObjectPixelIterator& other) const {
            return !(*this == other);
        }

    private:
        bool inRect() const {
            return m_current.x >= m_rect.x && m_current.x < m_rect.x + m_rect.width && m_current.y >= m_rect.y &&
                m_current.y < m_rect.y + m_rect.height;
        }

        bool inObject() const {
            return m_tile.at<int32_t>(m_current) == m_object->m_id;
        }

        void next() {
            m_current.x++;
            if (m_current.x >= m_rect.x + m_rect.width) {
                m_current.x = m_rect.x;
                m_current.y++;
            }
        }

        bool moveToValid() {
            while (inRect() && !inObject()) {
                next();
            }
            return inRect();
        }
    };
}
