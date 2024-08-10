// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS OTSmallScene : public OTScene
        {
        public:
            OTSmallScene(
                const std::string& filePath,
                const std::string& name,
                const slideio::TiffDirectory& dir,
                bool auxiliary=true);
            cv::Rect getRect() const override;
            int getNumChannels() const override;
            void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                            cv::OutputArray output) override;
        private:
            slideio::TiffDirectory m_directory;
            LevelInfo m_levelInfo;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif