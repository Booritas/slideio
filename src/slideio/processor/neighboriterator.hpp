// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once

#include "slideio/processor/slideio_processor_def.hpp"
#include "slideio/processor/imageobject.hpp"
#include "slideio/processor/processortools.hpp"
#include "slideio/processor/tile.hpp"
#include <opencv2/core/mat.hpp>

#include "slideio/base/exceptions.hpp"

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
        std::queue<ImageObject*> m_neighbors;

        ImageObject* m_lastNeighbor = nullptr;
        ImageObjectManager* m_objectManager = nullptr;
        bool m_end;
        Tile m_tile;
        bool m_lastStep;
        bool m_8neighbors = true;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = cv::Point;
        using difference_type = std::ptrdiff_t;
        using pointer = cv::Point*;
        using reference = cv::Point&;

        NeighborIterator(ImageObject* object, ImageObjectManager* objManager, cv::Mat& tile, const cv::Point& tileOrg, bool diagNeighbors,
                         bool begin) :
            m_object(object),
            m_objectManager(objManager),
            m_end(!begin),
            m_tile(tile, tileOrg),
            m_lastStep(false),
            m_8neighbors(diagNeighbors) {

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
            return getCurrentNeighbor();
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

            if(!m_neighbors.empty()) {
                m_neighbors.pop();
                while (!m_neighbors.empty()) {
                    auto nexNeighbor = m_neighbors.front();
                    if (m_neighbors.front() != m_lastNeighbor) {
                        return true;
                    }
                    m_neighbors.pop();
                }
            }

            if (m_lastStep) {
                return false;
            }

            while(nextNeighbor()) {
                if(m_current == m_start) {
                    m_lastStep = true;
                }
                if(m_neighbors.front() != m_lastNeighbor) {
                    m_lastNeighbor = m_neighbors.front();
                    return true;
                }
            }

            return false;
        }

        ImageObject* getCurrentNeighbor() const {
            return m_neighbors.front();
        }

        int32_t findDiagNeighborId(const cv::Point& p1, const cv::Point& p2) const {
            if(p1.x == p2.x) {
                // vertical line
                if(p1.y > p2.y) {
                    cv::Point pn(p2.x-1, p2.y-1);
                    pn -= m_tile.getOffset();
                    return m_tile.getMask().at<int32_t>(pn);
                }
                else {
                    cv::Point pn(p2.x, p2.y);
                    pn -= m_tile.getOffset();
                    return m_tile.getMask().at<int32_t>(pn);
                }
            }
            else if(p1.y == p2.y) {
                // horizontal line
                if (p1.x < p2.x) {
                    cv::Point pn(p2.x, p2.y-1);
                    pn -= m_tile.getOffset();
                    return m_tile.getMask().at<int32_t>(pn);
                }
                else {
                    cv::Point pn(p2.x-1, p2.y);
                    pn -= m_tile.getOffset();
                    return m_tile.getMask().at<int32_t>(pn);
                }
            }
            return -1;
        }

        bool nextNeighbor() {
            if(!m_neighbors.empty()) {
                RAISE_RUNTIME_ERROR << "Neighbor iterator unexpected error: NOT empty neighbor queue";
            }
            cv::Point prev = m_prev;
            const int32_t id = m_object->m_id;
            for (int i = 0; i < 3; i++) {
                const cv::Point nextPoint = ProcessorTools::rotatePointCW(prev, m_current);
                const cv::Point nextLocal = nextPoint - m_tile.getOffset();
                if (nextLocal.x >= 0 && nextLocal.y >= 0 && nextLocal.x <= m_tile.getWidth() && nextLocal.y <= m_tile.getHeight()) {
                    const int32_t neighborId = m_tile.getLineNeighborId(nextPoint, m_current, id);
                    if (neighborId >= 0) {
                        m_prev = m_current;
                        m_current = nextPoint;
                        if (neighborId != 0) {
                            ImageObject* neighbor = m_objectManager->getObjectPtr(neighborId);
                            m_neighbors.push(neighbor);
                            if(m_8neighbors) {
                                int32_t diagId = findDiagNeighborId(m_prev, m_current);
                                if(diagId > 0) {
                                    ImageObject* diagNeighbor = m_objectManager->getObjectPtr(diagId);
                                    if(diagNeighbor != m_neighbors.front()) {
                                        m_neighbors.push(diagNeighbor);
                                    }
                                } else if(diagId == 0) {
                                    if (nullptr != m_neighbors.front()) {
                                        m_neighbors.push(nullptr);
                                    }
                                }
                            }
                        }
                        return true;
                    }
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
                    const cv::Point point = cv::Point(x, y);
                    if (id == m_tile.getMask().at<int32_t>(point.y, point.x)) {
                        startPixel = point + cv::Point(0, 1) + m_tile.getOffset();
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
