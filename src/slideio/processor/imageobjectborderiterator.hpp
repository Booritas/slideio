// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/processor/slideio_processor_def.hpp"
#include "slideio/processor/imageobject.hpp"
#include "slideio/processor/processortools.hpp"
#include "slideio/processor/tile.hpp"
#include <opencv2/core/mat.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    class SLIDEIO_PROCESSOR_EXPORTS ImageObjectBorderIterator
    {
    private:
        cv::Point m_current;
        cv::Point m_prev;
        cv::Point m_start;
        ImageObject* m_object;
        bool m_end;
        Tile m_tile;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = cv::Point;
        using difference_type = std::ptrdiff_t;
        using pointer = cv::Point*;
        using reference = cv::Point&;

        ImageObjectBorderIterator(ImageObject* object, cv::Mat& tile, const cv::Rect& tileRect, bool begin) :
            m_object(object),
            m_tile(tile, tileRect.tl()), m_end(!begin) {
            if (begin) {
                if (m_tile.findFirstObjectBorderPixel(m_object, m_current)) {
                    m_prev = m_current + cv::Point(0, 1);
                    m_start = m_current;
                    m_end = false;
                }
                else {
                    m_end = true;
                }
            }
        }

        const cv::Point& operator*() const {
            return m_current;
        }

        ImageObjectBorderIterator& operator++() {
            m_end = !m_tile.findNextObjectBorderPixel(m_object, cv::Point(m_prev), cv::Point(m_current),
                m_prev, m_current);
            if(!m_end) {
                m_end = m_current == m_start;
            }
            return *this;
        }

        ImageObjectBorderIterator operator++(int) {
            ImageObjectBorderIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ImageObjectBorderIterator& other) const {
            if (m_end && other.m_end) {
                return true;
            }
            if (!m_end && !other.m_end) {
                return m_current == other.m_current;
            }
            return false;
        }

        bool operator!=(const ImageObjectBorderIterator& other) const {
            return !(*this == other);
        }
    };
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
