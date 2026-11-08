// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/core/cvscene.hpp"
#include <memory>
#include <string>

namespace slideio
{
    /**@brief one associated JPEG -- macro, label or thumbnail -- as a single-level RGB scene.
     *
     * Its size comes from the JPEG header, read when the slide is opened; the pixels are
     * decoded only when a block is read. It shares the dataset, so it outlives the slide.*/
    class SLIDEIO_MRXS_EXPORTS MRXSAuxScene : public CVScene
    {
    public:
        MRXSAuxScene(std::shared_ptr<const mrxs::MRXSDataset> dataset, std::string name, mrxs::PayloadRef payload);

        std::string getFilePath() const override;
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override;
        std::string getName() const override { return m_name; }
        cv::Rect getRect() const override { return cv::Rect(0, 0, m_size.width, m_size.height); }
        int getNumChannels() const override { return 3; }
        DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override { return Resolution(0, 0); }
        double getMagnification() const override { return 0; }
        Compression getCompression() const override { return Compression::Jpeg; }
        /// Reads share only the dataset's self-synchronised runtime.
        bool supportsConcurrentReads() const override { return true; }
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
                                          cv::OutputArray output) override;

    private:
        std::shared_ptr<const mrxs::MRXSDataset> m_dataset;
        std::string m_name;
        mrxs::PayloadRef m_payload;
        cv::Size m_size;
    };
}
