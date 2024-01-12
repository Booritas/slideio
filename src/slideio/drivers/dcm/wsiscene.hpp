// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/drivers/dcm/dcmfile.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class DCMSlide;
    class SLIDEIO_DCM_EXPORTS WSIScene : public CVScene, public Tiler
    {
    public:
        struct TilerData
        {
            int zoomLevelIndex = 0;
            int zSliceIndex = 0;
            int tFrameIndex = 0;
            double relativeZoom = 1.;
        };
    public:
        WSIScene();
        void addFile(std::shared_ptr<DCMFile>& file);
        void init();
        std::string getFilePath() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
    private:
        std::shared_ptr<DCMFile> getBaseFile() const;

    public:
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
            cv::OutputArray output) override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;

    private:
        std::vector<std::shared_ptr<DCMFile>> m_files;
        cv::Rect m_rect = { 0, 0, 0, 0 };
        std::string m_name;
        int m_numSlices = 1;
        int m_numFrames = 1;
        int m_numChannels = 0;
        std::string m_filePath;
        DataType m_dataType = DataType::DT_Unknown;
        Compression m_compression = Compression::Unknown;
        double m_magnification = 0;
        Resolution m_resolution = { 0, 0 };
    };
};