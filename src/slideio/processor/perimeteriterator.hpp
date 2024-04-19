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

namespace slideio {
    class SLIDEIO_PROCESSOR_EXPORTS PerimeterIterator {
    private:
        cv::Point m_current;
        cv::Point m_prev;
        cv::Point m_start;
        ImageObject *m_object;
        bool m_end;
        Tile m_tile;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = cv::Point;
        using difference_type = std::ptrdiff_t;
        using pointer = cv::Point *;
        using reference = cv::Point &;

        PerimeterIterator(ImageObject *object, cv::Mat &tile, const cv::Point &tileOrg, bool begin) :
                m_object(object),
                m_tile(tile, tileOrg), m_end(!begin) {
            if (begin) {
                if (m_tile.findFirstObjectBorderPixel(m_object, m_current)) {
                    m_prev = m_current + cv::Point(0, 1);
                    m_start = m_current;
                    m_end = false;
                } else {
                    m_end = true;
                }
            }
        }

        const cv::Point &operator*() const {
            return m_current;
        }

        PerimeterIterator &operator++() {
            m_end = !next();
            if (!m_end) {
                m_end = m_current == m_start;
            }
            return *this;
        }

        PerimeterIterator operator++(int) {
            PerimeterIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const PerimeterIterator &other) const {
            if (m_end && other.m_end) {
                return true;
            }
            if (!m_end && !other.m_end) {
                return m_current == other.m_current;
            }
            return false;
        }

        bool operator!=(const PerimeterIterator &other) const {
            return !(*this == other);
        }

    private:

        bool next() {
            static cv::Point neighbors[3][3] = {
            {{0, 0},  {-1, 0}, {0, 0}},
            {{0, -1}, {0,  0}, {0, 1}},
            {{0, 0},  {1,  0}, {0, 0}}
            }
            cv::Point current = m_current;
            cv::Point prev = m_prev;
            for(int i=0; i<3; i++) {
                cv::Point offset = current - prev + cv::Point(1, 1);
                cv::Point next = current + neighbors[offset.y][offset.x];
                if(isPerimeterLine(current, next)) {
                    m_prev = m_current;
                    m_current = next;
                    return true;
                }
                prev = next;
            }
            return false;
        }

        bool isPerimeterLine(const cv::Point& first, const cv::Point& second) const {
            const cv::Point p1 = first - m_tile.getOffset();
            const cv::Point p2 = second - m_tile.getOffset();
            if(p1.x<=0 || p1.y<=0 || p2.x<=0 || p2.y<=0) {
                return true;
            }
            const cv::Mat& mask = m_tile.getMask();
            cv::Point pix1, pix2;
            if(p1.x == p2.x) {
                const int y = std::min(p1.y, p2.y);
                pix1 = cv::Point(p1.x-1,y);
                pix2 = cv::Point(p1.x, y);
            }
            else if(p1.y == p2.y) {
                const int x = std::min(p1.x, p2.x);
                pix1 = cv::Point(x,p1.y-1);
                pix2 = cv::Point(x, p2.y);
            } else {
                return false;
            }

            int id1 = mask.at(pix1);
            int id2 = mask.at(pix2);
            return id1 != id2 && (id1 == m_object->m_id || id2 == m_object->m_id);
        }
    };
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
