// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/tilecomposer.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS OTTiledScene : public OTScene, public Tiler
        {
        public:
            OTTiledScene(const std::string& filePath,
                         const std::string& name,
                         const std::vector<slideio::TiffDirectory>& dirs);
            OTTiledScene(const std::string& filePath, 
                         libtiff::TIFF* hFile,
                         const std::string& name,
                         const std::vector<slideio::TiffDirectory>& dirs);
            int getNumChannels() const override;
            cv::Rect getRect() const override;
            void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                            cv::OutputArray output) override;
            int findZoomLevel(double zoom) const;
            // Tiler methods
            int getTileCount(void* userData) override;
            bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
            bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                          void* userData) override;
            void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
            std::string getChannelName(int channel) const override;
            bool isBrightField() const;
        private:
            void initialize();
            void initializeChannelNames();
            bool readTiffTile(int tileIndex, const TiffDirectory& dir, const std::vector<int>& channelIndices, cv::OutputArray tileRaster);
            bool readTiffDirectory(const TiffDirectory& dir, const std::vector<int>& channelIndices, cv::OutputArray tileRaster);
        private:
            std::vector<slideio::TiffDirectory> m_directories;
            bool m_isUnmixed = false;
            std::vector<int> m_zoomDirectoryIndices;
            int m_numChannels = 0;
            std::vector<std::string> m_channelNames;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
