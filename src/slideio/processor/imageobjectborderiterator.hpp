// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <opencv2/core/mat.hpp>
#include "slideio/processor/slideio_processor_def.hpp"
#include "slideio/processor/imageobject.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_PROCESSOR_EXPORTS ImageObjectBorderIterator
    {
    private:
        cv::Point m_current;
        cv::Point m_prev;
        ImageObject* m_object;
        cv::Rect m_rect;
        cv::Mat m_tile;
        bool m_end;
        static const cv::Point neighbors[8];

        enum class Direction
        {
            LEFT = 1,
            UP = 2,
            RIGHT = 3,
            DOWN = 4
        };

        Direction m_direction = Direction::LEFT;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = cv::Point;
        using difference_type = std::ptrdiff_t;
        using pointer = cv::Point*;
        using reference = cv::Point&;

        ImageObjectBorderIterator(ImageObject* object, cv::Mat& tile, const cv::Rect&  objectRect, const cv::Point& tileOffset, bool begin) :
            m_object(object), m_rect(objectRect), m_tile(tile), m_end(!begin) {
            m_current = m_object->m_innerPoint - tileOffset;
            m_prev = {-1, -1};
            if (begin) {
                m_end = m_rect.area() <= 0 || !isBorder(m_current);
            }
        }

        const cv::Point& operator*() const {
            return m_current;
        }

        ImageObjectBorderIterator& operator++() {
            m_end = !next();
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

    private:
        void turn() {
            switch (m_direction) {
            case Direction::LEFT:
                m_direction = Direction::DOWN;
                return;
            case Direction::DOWN:
                m_direction = Direction::RIGHT;
                return;
            case Direction::RIGHT:
                m_direction = Direction::UP;
                return;
            case Direction::UP:
                m_direction = Direction::LEFT;
                return;
            }
        }

        cv::Point direction(Direction direction) const {
            switch (m_direction) {
            case Direction::LEFT:
                return {-1, 0};
            case Direction::DOWN:
                return {0, 1};
            case Direction::RIGHT:
                return {1, 0};
            case Direction::UP:
                return {0, -1};
            }
            return {0, 0};
        }

        bool inRect() const {
            return m_current.x >= m_rect.x && m_current.x < m_rect.x + m_rect.width && m_current.y >= m_rect.y &&
                m_current.y < m_rect.y + m_rect.height;
        }

        bool inObject() const {
            return m_tile.at<int32_t>(m_current) == m_object->m_id;
        }

        bool next() {
            int count = 4;
            while (count--) {
                cv::Point dir = direction(m_direction);
                cv::Point nextPoint = m_current + dir;
                if (nextPoint == m_object->m_innerPoint) {
                    return false;
                }
                if (nextPoint != m_prev) {
                    if (isBorder(nextPoint)) {
                        m_prev = m_current;
                        m_current = nextPoint;
                        return true;
                    }
                }
                turn();
            }
            return false;
        }

        bool isInObject(const cv::Point& point) const {
            return m_tile.at<int32_t>(point) == m_object->m_id;
        }

        bool isBorder(const cv::Point& point) const {
            if (inRect() && isInObject(point)) {
                if (point.x == 0 || point.x == (m_tile.cols - 1) || point.y == 0 || point.y == (m_tile.rows - 1)) {
                    return true;
                }
                for (const cv::Point& neighbor : neighbors) {
                    const cv::Point neighborPoint = point + neighbor;
                    if (neighborPoint.x >= 0
                        && neighborPoint.x < m_tile.cols
                        && neighborPoint.y >= 0
                        && neighborPoint.y < m_tile.rows
                        && isInObject(neighborPoint)) {
                        return true;
                    }
                }
            }
            return false;
        }
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
