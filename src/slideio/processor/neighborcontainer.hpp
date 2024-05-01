// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <opencv2/core/mat.hpp>
#include "slideio/processor/imageobject.hpp"
#include "slideio/processor/neighboriterator.hpp"

namespace slideio
{
    class ImageObjectManager;

    class NeighborContainer
    {
    public:
        NeighborContainer(ImageObject* object, ImageObjectManager* objMngr, cv::Mat& tile, cv::Point tileOrg, bool diagNeighbors=true) :
                m_object(object), m_tile(tile), m_tileOrg(tileOrg), m_objManager(objMngr), m_diagNeighbors(diagNeighbors) {
        }

        NeighborIterator begin() {
            return NeighborIterator(m_object, m_objManager, m_tile, m_tileOrg, m_diagNeighbors, true);
        }
        NeighborIterator end() {
            return NeighborIterator(m_object, m_objManager, m_tile, m_tileOrg, m_diagNeighbors, false);
        }
    private:
        ImageObject* m_object;
        cv::Mat m_tile;
        cv::Point m_tileOrg;
        ImageObjectManager* m_objManager;
        bool m_diagNeighbors;
    };
}
