// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_gdalscene_HPP
#define OPENCV_slideio_gdalscene_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/CVScene.hpp"
#include "slideio/structs.hpp"
#include <opencv2/core.hpp>
#include "slideio/gdal_lib.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS GDALScene : public slideio::CVScene
    {
    public:
        GDALScene(const std::string& filePath);
        GDALScene(GDALDatasetH ds, const std::string& filePath);
        virtual ~GDALScene();
        std::string getFilePath() const override;
        int getNumChannels() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        slideio::Resolution getResolution() const override;
        double getMagnification() const override;
        static GDALDatasetH openFile(const std::string& filePath);
        static void closeFile(GDALDatasetH hfile);
        static slideio::DataType dataTypeFromGDALDataType(GDALDataType dt);
        std::string getName() const override;
        cv::Rect getRect() const override;
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
    private:
        GDALDatasetH m_hFile;
        std::string m_filePath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif

