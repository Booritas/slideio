// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "vsifile.hpp"
#include "vsiscene.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{

    class SLIDEIO_VSI_EXPORTS VsiFileScene : public VSIScene
    {
    public:
        VsiFileScene(const std::string& filePath, std::shared_ptr<vsi::VSIFile>& vsiFile, int directoryIndex);
    public:
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, cv::OutputArray output) override;
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
    protected:
        void init();
    protected:
        int m_directoryIndex;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
