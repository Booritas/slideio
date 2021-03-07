// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmscene_HPP
#define OPENCV_slideio_dcmscene_HPP
#include "slideio/core/cvscene.hpp"
#include "dcmfile.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class DCMSlide;
    class SLIDEIO_EXPORTS DCMScene : public CVScene
    {
    public:
        DCMScene();
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
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        void readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
            cv::OutputArray output) override;
        std::string getName() const override;
        Compression getCompression() const override;
        void addFile(std::shared_ptr<DCMFile>& file);
        void init();
    private:
        std::vector<std::shared_ptr<DCMFile>> m_files;
        cv::Rect m_rect = { 0, 0, 0, 0 };
        std::string m_name;
        int m_numSlices = 1;
        int m_numFrames = 1;
        int m_numChannels = 0;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
