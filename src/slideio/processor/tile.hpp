// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/processor/slideio_processor_def.hpp"
#include <opencv2/core/mat.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    class SLIDEIO_PROCESSOR_EXPORTS Tile
    {
    public:
        Tile(const cv::Mat& mask, const cv::Point& offset) {
            m_mask = mask;
            m_offset = offset;
        }

        bool contains(const cv::Point& point) const {
            const cv::Rect tileRect(m_offset, m_mask.size());
            return tileRect.contains(point);
        }

        bool doesObjectContain(const cv::Point& point, const ImageObject* object) const {
            if (object->m_boundingRect.contains(point) && contains(point)) {
                return m_mask.at<int32_t>(point - m_offset) == object->m_id;
            }
            return false;
        }

        bool isOnObjectBorder(const cv::Point& point, const ImageObject* object) const {
            if (doesObjectContain(point, object)) {
                if (point.x == 0 || point.x == (m_mask.cols - 1) || point.y == 0 || point.y == (m_mask.rows - 1)) {
                    return true;
                }
                // find a neighbor that is not in the object
                cv::Point center = point;
                cv::Point nextPoint = point - cv::Point(0, 1);
                for (int neighbor = 0; neighbor < 7; ++neighbor) {
                    nextPoint = ProcessorTools::nextMoveCW(nextPoint, center);
                    if (!doesObjectContain(nextPoint, object)) {
                        return true;
                    }
                }
            }
            return false;
        }

        cv::Rect getObjectTileRect(const ImageObject* ImageObject) const {
            const cv::Rect tileRect(m_offset, m_mask.size());
            const cv::Rect rc = tileRect & ImageObject->m_boundingRect;
            return rc - m_offset;
        }

        bool findFirstObjectBorderPixel(const ImageObject* object, cv::Point& borderPixel) const {
            cv::Point point = object->m_innerPoint;
            if (isOnObjectBorder(point, object)) {
                borderPixel = point;
                return true;
            }
            const cv::Rect objectTileRect = getObjectTileRect(object);
            const int beginY = objectTileRect.y;
            const int endY = objectTileRect.y + objectTileRect.height;
            const int beginX = objectTileRect.x;
            const int endX = objectTileRect.x + objectTileRect.width;
            for (int x = beginX; x < endX; ++x) {
                for (int y = endY - 1; y >= beginY; --y) {
                    point = m_offset + cv::Point(x, y);
                    if (isOnObjectBorder(point, object)) {
                        borderPixel = point;
                        return true;
                    }
                }
            }
            return false;
        }

        bool findNextObjectBorderPoint(const ImageObject* object, const cv::Point& start, const cv::Point& center,
                                       cv::Point& nextStart, cv::Point& nextCenter) const {
            cv::Point nextPoint = start;
            cv::Point prev = start;
            for (int neighbor = 0; neighbor < 7; ++neighbor) {
                prev = nextPoint;
                nextPoint = ProcessorTools::nextMoveCW(nextPoint, center);
                if (doesObjectContain(nextPoint, object)) {
                    nextStart = prev;
                    nextCenter = nextPoint;
                    return true;
                }
            }
            return false;
        };

    private:
        cv::Mat m_mask;
        cv::Point m_offset;
    };
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
