// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpiscene_HPP
#define OPENCV_slideio_ndpiscene_HPP

#include "ndpitifftools.hpp"
#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tilecomposer.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class NDPIFile;
}

namespace slideio
{
    class SLIDEIO_NDPI_EXPORTS NDPIScene : public CVScene, public Tiler
    {
        friend class NDPISlide;
    protected:
        NDPIScene();
    public:
        virtual ~NDPIScene();
        void init(const std::string& name, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex);
        int getNumChannels() const override;
        cv::Rect getRect() const override;
        std::string getFilePath() const override;
        std::string getName() const override {
            return m_sceneName;
        }
        slideio::DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
        void readResampledBlockChannels(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize, const std::vector<int>& channelIndices,
                                        cv::OutputArray output) override;
        const NDPITiffDirectory& findZoomDirectory(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize) const;
        void scaleBlockToDirectory(const cv::Rect& imageBlockRect, const slideio::NDPITiffDirectory& dir, cv::Rect& dirBlockRect) const;
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                      void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
    private:
        void makeSureValidDirectoryType(NDPITiffDirectory::Type directoryType);
    protected:
        NDPIFile* m_pfile;
        int m_startDir;
        int m_endDir;
        std::string m_sceneName;
        cv::Rect m_rect;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif