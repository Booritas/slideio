// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_gdalscene_HPP
#define OPENCV_slideio_gdalscene_HPP

#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/base/slideio_enums.hpp"
#include <opencv2/core.hpp>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SmallImagePage;

    class SLIDEIO_GDAL_EXPORTS GDALScene : public slideio::CVScene
    {
    public:
        GDALScene(SmallImagePage* image, const std::string& filePath);
        virtual ~GDALScene() = default;
        std::string getFilePath() const override;
        int getNumChannels() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        slideio::Resolution getResolution() const override;
        double getMagnification() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        Compression getCompression() const override;
        MetadataFormat getMetadataFormat() const override;
        std::string getRawMetadata() const override;
    private:
        SmallImagePage* m_imagePage;
        std::string m_filePath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif

