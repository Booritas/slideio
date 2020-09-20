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
            ImageItem() : m_ItemIndex(-1), m_ZIndex(-1), m_CIndex(-1), m_TIndex(-1),
                          m_PositionIndex(-1), m_SceneIndex(-1) {
            }
            int getZIndex() const { return m_ZIndex; }
            int getCIndex() const { return m_CIndex; }
            int getTIndex() const { return m_TIndex; }
            int getPositionIndex() const { return m_PositionIndex; }
            int getSceneIndex() const { return m_SceneIndex; }
            int getItemIndex() const { return m_ItemIndex; }
            void setItemIndex(int itemIndex) { m_ItemIndex = itemIndex; }
            void setZIndex(int zIndex) { m_ZIndex = zIndex; }
            void setCIndex(int cIndex) { m_CIndex = cIndex; }
            void setTIndex(int tIndex) { m_TIndex = tIndex; }
            void setPositionIndex(int positionIndex) { m_PositionIndex = positionIndex; }
            void setSceneIndex(int sceneIndex) { m_SceneIndex = sceneIndex; }
        private:
            int m_ItemIndex;
            int m_ZIndex;
            int m_CIndex;
            int m_TIndex;
            int m_PositionIndex;
            int m_SceneIndex;
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
        void readImageItem(ImageItem& item);
        void readImageItems();
        void init();
        void parseImageTags();
        void parseImageInfo();
    private:
        std::string m_filePath;
        ole::compound_document m_Doc;
        int m_Width;
        int m_Height;
        PixelFormat m_PixelFormat;
        int m_RawCount;
        int m_ChannelCount;
        DataType m_ChannelDataType;
        std::vector<std::string> m_ChannelNames;
        std::vector<ImageItem> m_ImageItems;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
