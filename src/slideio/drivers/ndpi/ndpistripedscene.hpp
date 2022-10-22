// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#ifndef OPENCV_slideio_ndpistripedscene_HPP
#define OPENCV_slideio_ndpistripedscene_HPP

#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{

    class SLIDEIO_NDPI_EXPORTS NDPIStripedScene : public NDPIScene
    {
    public:
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif