// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/scenemetadata.hpp"
#include "slideio/slideio/scenesource.hpp"
#include "slideio/slideio/slideio_def.hpp"
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace slideio
{
    /**@brief A CVScene whose pixels come from an ISceneSource.
     *
     * Internal. Consumers reach it only through makeScene(), which is what
     * keeps OpenCV out of the public surface: this header includes
     * cvscene.hpp and is never installed.
     */
    class SLIDEIO_EXPORTS SourceCVScene : public CVScene
    {
    public:
        /// Driver id every SourceCVScene reports. No registered driver has it,
        /// so openSlide() cannot re-open such a scene; code that would re-open a
        /// scene by (file path, driver id) checks for it and shares the scene instead.
        static constexpr const char* DRIVER_ID = "SOURCE";

        SourceCVScene(std::shared_ptr<ISceneSource> source, SceneMetadata metadata);

        std::string getFilePath() const override;
        std::string getName() const override;
        int getSceneIndex() const override;
        const std::string& getDriverId() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        int getNumZSlices() const override;
        int getNumTFrames() const override;
        DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        int getChannelSignificantBits(int channel) const override;
        double getMagnification() const override;
        Compression getCompression() const override;
        ColorProfile getColorProfile() const override;
        int getNumZoomLevels() const override;
        const LevelInfo* getZoomLevelInfo(int level) const override;
        bool supportsConcurrentReads() const override;
        std::string getRawMetadata() const override;
        const std::list<std::string>& getAuxImageNames() const override;
        int getNumAuxImages() const override;
        std::shared_ptr<CVScene> getAuxImage(const std::string& imageName) const override;

        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;

        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;

    private:
        cv::Mat prepareBlock(const cv::Rect& rect, const char* rectLabel,
            const cv::Size& blockSize, const std::vector<int>& requested,
            int zSliceIndex, int tFrameIndex,
            std::vector<int>& channels, cv::OutputArray output) const;

        std::shared_ptr<ISceneSource> m_source;
        SceneMetadata m_metadata;
        std::string m_driverId;
        mutable std::once_flag m_auxNamesOnce;
    };
}
