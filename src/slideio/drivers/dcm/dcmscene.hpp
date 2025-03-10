﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmscene_HPP
#define OPENCV_slideio_dcmscene_HPP

#include <map>

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/drivers/dcm/dcmfile.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class DCMSlide;
    class SLIDEIO_DCM_EXPORTS DCMScene : public CVScene
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
        DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        std::string getName() const override;
        Compression getCompression() const override;
        void addFile(std::shared_ptr<DCMFile>& file);
        void init();
        std::string getRawMetadata() const override;
    protected:
        void prepareSliceIndices();
        void checkScene();
        void extractSliceRaster(const cv::Mat& frame,
                                const cv::Rect& blockRect,
                                const cv::Size& blockSize,
                                const std::vector<int>& componentIndices,
                                cv::OutputArray output);
        std::pair<int, int> findFileIndex(int zSliceIndex);
    private:
        std::vector<std::shared_ptr<DCMFile>> m_files;
        std::map<int, int> m_sliceMap;
        cv::Rect m_rect = { 0, 0, 0, 0 };
        std::string m_name;
        int m_numSlices = 1;
        int m_numFrames = 1;
        int m_numChannels = 0;
        std::string m_filePath;
        DataType m_dataType = DataType::DT_Unknown;
        Compression m_compression = Compression::Unknown;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
