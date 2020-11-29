// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zviscene_HPP
#define OPENCV_slideio_zviscene_HPP

#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tilecomposer.hpp"
#include "slideio/drivers/zvi/zviimageitem.hpp"
#include <pole/storage.hpp>

#include "zvitile.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class ZVISlide;

    class SLIDEIO_EXPORTS ZVIScene : public CVScene, public Tiler
    {
    private:
        struct TilerData
        {
            int zSliceIndex = 0;
        };
    public:
        ZVIScene(const std::string& filePath);
        std::string getFilePath() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        int getNumZSlices() const override;
        int getNumTFrames() const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        void validateChannelIndex(int channel) const;
        slideio::DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
                                        const std::vector<int>& componentIndices, cv::OutputArray output) override;
        std::string getName() const override;
        Compression getCompression() const override;
    protected:
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
    public:
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                      void* userData) override;
    private:
        ZVIPixelFormat getPixelFormat() const;
        void alignChannelInfoToPixelFormat();
        void computeSceneDimensions();
        void readImageItems();
        void init();
        void parseImageTags();
        void parseImageInfo();
        void computeTiles();
    private:
        std::string m_filePath;
        ole::compound_document m_Doc;
        int m_Width = 0;
        int m_Height = 0;
        int m_RawCount = 0;
        ZVIPixelFormat m_PixelFormat = ZVIPixelFormat::PF_UNKNOWN;
        int m_ChannelCount = 0;
        int m_ZSliceCount = 0;
        int m_TFrameCount = 0;
        int m_TileCountX = 1;
        int m_TileCountY = 1;
        std::vector<DataType> m_ChannelDataTypes;
        std::vector<std::string> m_ChannelNames;
        std::vector<ZVIImageItem> m_ImageItems;
        std::vector<ZVITile> m_Tiles;
        Resolution m_res = {0,0};
        double m_ZSliceRes = 0.;
        std::string m_SceneName;
        Compression m_Compression = Compression::Uncompressed;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
