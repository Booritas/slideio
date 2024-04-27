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
    class NeighborIterator
    {
    private:
        cv::Point m_current;
        cv::Point m_prev;
        cv::Point m_start;
        ImageObject* m_object;
        ImageObject* m_currentNeighbor = nullptr;
        ImageObject* m_lastNeighbor = nullptr;
        ImageObjectManager* m_objectManager = nullptr;
        bool m_end;
        Tile m_tile;
        bool m_lastStep;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = cv::Point;
        using difference_type = std::ptrdiff_t;
        using pointer = cv::Point*;
        using reference = cv::Point&;

        NeighborIterator(ImageObject* object, ImageObjectManager* objManager, cv::Mat& tile, const cv::Point& tileOrg,
                         bool begin) :
            m_object(object),
            m_objectManager(objManager),
            m_end(!begin),
            m_tile(tile, tileOrg),
            m_lastStep(false) {
            if (begin) {
                if (findStartPoint(m_object, m_current)) {
                    m_prev = m_current + cv::Point(0, 1);
                    m_start = m_current;
                    m_end = !next();
                }
                else {
                    m_end = true;
                }
            }
        }

        ImageObject* operator*() const {
            return m_currentNeighbor;
        }

        NeighborIterator& operator++() {
            m_end = !next();
            return *this;
        }

        NeighborIterator operator++(int) {
            NeighborIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const NeighborIterator& other) const {
            if (m_end && other.m_end) {
                return true;
            }
            if (!m_end && !other.m_end) {
                return m_current == other.m_current;
            }
            return false;
        }

        bool operator!=(const NeighborIterator& other) const {
            return !(*this == other);
        }

    private:

        bool next() {
            if(m_lastStep) {
                return false;
            }
            while(nextNeighbor()) {
                if(m_current == m_start) {
                    m_lastStep = true;
                }
                if(m_currentNeighbor != m_lastNeighbor) {
                    m_lastNeighbor = m_currentNeighbor;
                    return true;
                }
            }
            return false;
        }

        bool nextNeighbor() {
            cv::Point prev = m_prev;
            const int32_t id = m_object->m_id;
            for (int i = 0; i < 3; i++) {
                const cv::Point nextPoint = ProcessorTools::rotatePointCW(prev, m_current);
                const int32_t neighborId = m_tile.getLineNeighborId(nextPoint, m_current, id);
                if (neighborId>=0) {
                    m_prev = m_current;
                    m_current = nextPoint;
                    if(neighborId != 0) {
                        m_currentNeighbor = m_objectManager->getObjectPtr(neighborId);
                    }
                    return true;
                }
                prev = nextPoint;
            }
            return false;
        }

        bool findStartPoint(const ImageObject* object, cv::Point& startPixel) const {
            const cv::Rect objectTileRect = object->m_boundingRect - m_tile.getOffset();
            const int beginY = objectTileRect.y;
            const int endY = objectTileRect.y + objectTileRect.height;
            const int beginX = objectTileRect.x;
            const int endX = objectTileRect.x + objectTileRect.width;
            const int32_t id = object->m_id;
            for (int x = beginX; x < endX; ++x) {
                for (int y = endY - 1; y >= beginY; --y) {
                    const cv::Point point = m_tile.getOffset() + cv::Point(x, y);
                    if (id == m_tile.getMask().at<int32_t>(point.x, point.y)) {
                        startPixel = point + cv::Point(0, 1);
                        return true;
                    }
                }
            }
            return false;
        }
    };
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
