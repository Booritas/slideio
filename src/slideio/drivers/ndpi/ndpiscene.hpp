// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpiscene_HPP
#define OPENCV_slideio_ndpiscene_HPP

#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tilecomposer.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_NDPI_EXPORTS NDPIScene : public CVScene, public Tiler
    {
    public:
        NDPIScene(const std::string& filePath,
            const std::string& name);
        int getNumChannels() const override;
        cv::Rect getRect() const override;
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices,
            cv::OutputArray output) override;
        //const slideio::TiffDirectory& findZoomDirectory(double zoom) const;
        // Tiler methods
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
    private:
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif