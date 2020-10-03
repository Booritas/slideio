// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zviscene_HPP
#define OPENCV_slideio_zviscene_HPP
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tilecomposer.hpp"
#include "slideio/drivers/czi/czisubblock.hpp"
#include "slideio/drivers/czi/czistructs.hpp"
#include <map>
#include <pole/storage.hpp>

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
        enum class PixelFormat
        {
            PF_UNKNOWN,
            PF_BGR,
            PF_BGRA,
            PF_UINT8,
            PF_INT16,
            PF_INT32,
            PF_FLOAT,
            PF_DOUBLE,
            PF_BGR16,
            PF_BGR32
        };

        class ImageItem
        {
        public:
            ImageItem() = default;
            void setItemIndex(int itemIndex) { m_ItemIndex = itemIndex; }
            int getZIndex() const { return m_ZIndex; }
            int getCIndex() const { return m_CIndex; }
            int getTIndex() const { return m_TIndex; }
            int getPositionIndex() const { return m_PositionIndex; }
            int getSceneIndex() const { return m_SceneIndex; }
            int getItemIndex() const { return m_ItemIndex; }
            std::streamoff getDataOffset() { return m_DataPos; }
            std::string getChannelName() const { return m_ChannelName; }
            PixelFormat getPixelFormat() const { return m_PixelFormat; }
            int getWidth() const { return m_Width; }
            int getHeight() const { return m_Height; }
            int getZSliceCount() const { return m_ZSliceCount; }
            void setZSliceCount(int depth) { m_ZSliceCount = depth; }
            DataType getDataType() const { return m_DataType; }
            int getChannelCount() const { return m_ChannelCount; }
            void readItemInfo(ole::compound_document& doc);
        private:
            void setWidth(int width) { m_Width = width; }
            void setHeight(int height) { m_Height = height; }
            void setChannelCount(int channels) { m_ChannelCount = channels; }
            void setDataType(DataType dt) { m_DataType = dt; }
            void setPixelFormat(PixelFormat pixelFormat) { m_PixelFormat = pixelFormat; }
            void setZIndex(int zIndex) { m_ZIndex = zIndex; }
            void setCIndex(int cIndex) { m_CIndex = cIndex; }
            void setTIndex(int tIndex) { m_TIndex = tIndex; }
            void setPositionIndex(int positionIndex) { m_PositionIndex = positionIndex; }
            void setSceneIndex(int sceneIndex) { m_SceneIndex = sceneIndex; }
            void setDataOffset(std::streamoff pos) { m_DataPos = pos; }
            void setChannelName(const std::string& name) { m_ChannelName = name; }
            void readContents(ole::compound_document& doc);
            void readTags(ole::compound_document& doc);
        private:
            int m_ChannelCount = 0;
            int m_Width = 0;
            int m_Height = 0;
            int m_ItemIndex = -1;
            int m_ZIndex = -1;
            int m_CIndex = -1;
            int m_TIndex = -1;
            int m_PositionIndex = -1;
            int m_SceneIndex = -1;
            std::streamoff m_DataPos = 0;
            std::string m_ChannelName;
            PixelFormat m_PixelFormat = PixelFormat::PF_UNKNOWN;
            int m_ZSliceCount = 1;
            DataType m_DataType = DataType::DT_Unknown;
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
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                                          cv::OutputArray output);
        void readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& channelIndices, const cv::Range& zSliceRange,
                                          const cv::Range& timeFrameRange,
                                          cv::OutputArray output) override;
        std::string getName() const override;
        Compression getCompression() const override;
    public:
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                      void* userData) override;
    private:
        static DataType dataTypeFromPixelFormat(const PixelFormat pixel_format);
        static int channelCountFromPixelFormat(PixelFormat pixelFormat);
        void alignChannelInfoToPixelFormat();
        void computeSceneDimensions();
        void readImageItems();
        void init();
        void parseImageTags();
        void parseImageInfo();
    private:
        std::string m_filePath;
        ole::compound_document m_Doc;
        int m_Width;
        int m_Height;
        int m_RawCount;
        int m_ChannelCount;
        int m_ZSliceCount;
        int m_TFrameCount;
        std::vector<DataType> m_ChannelDataTypes;
        std::vector<std::string> m_ChannelNames;
        std::vector<ImageItem> m_ImageItems;
        Resolution m_res = {0,0};
        double m_ZSliceRes = 0.;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
