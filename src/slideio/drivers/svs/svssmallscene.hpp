// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_svssmallscene_HPP
#define OPENCV_slideio_svssmallscene_HPP

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/drivers/svs/svsscene.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_SVS_EXPORTS SVSSmallScene : public SVSScene
    {
    public:
        SVSSmallScene(
            const std::string& filePath,
            const std::string& name,
            const slideio::TiffDirectory& dir,
            bool auxiliary=true);
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
    private:
        slideio::TiffDirectory m_directory;
        LevelInfo m_levelInfo;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif