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
    /**@brief the slide's primary pyramid as one 8-bit RGB scene (design section 2).*/
    class SLIDEIO_MRXS_EXPORTS MRXSScene : public CVScene
    {
    public:
        explicit MRXSScene(std::shared_ptr<const mrxs::MRXSDataset> dataset);

        std::string getFilePath() const override;
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override { return 3; }
        DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
        double getChannelBackground(int channel) const override;
        /// Every read is request-local except MRXSRuntime's caches, which synchronise
        /// themselves without holding a lock across I/O or decoding (design section 8).
        bool supportsConcurrentReads() const override { return true; }

        /// {"mrxs.crop_to_content": "true" | "false"}, as opened: a re-open by path crops alike.
        DriverOptions getOpenOptions() const override;

        /// Selects the level as Tools::findZoomLevel does and reads it. Cropped, the
        /// rect is translated into nominal level-0 pixels before the level is scaled.
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
                                          cv::OutputArray output) override;
        /// The only rendering path: renders the integer level rectangle and resizes once.
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect, const cv::Size& blockSize,
                                               const std::vector<int>& channelIndices, int zSliceIndex,
                                               int tFrameIndex, cv::OutputArray output) override;

    protected:
        MetadataBuilder buildMetadataTree() const override;

    private:
        /// The part of the nominal level the scene presents: the crop box at that
        /// level, or the whole level when uncropped.
        cv::Rect levelBox(int level) const;
        void validateChannels(const std::vector<int>& channelIndices) const;
        /// Renders levelRect, in nominal level pixels, clamped to bounds.
        void readNominalLevel(int level, const cv::Rect& levelRect, const cv::Rect& bounds,
                              const cv::Size& blockSize, const std::vector<int>& channelIndices,
                              cv::OutputArray output) const;

        std::shared_ptr<const mrxs::MRXSDataset> m_dataset;
    };
}
