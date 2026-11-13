// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tiledareadownscale.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief one XY position of an ND2 file (design 4.6, 6, 7).
     *
     * Its state is the shared, immutable ImageInfo and FrameSource, so reads may
     * overlap, and a scene stays readable after its slide is gone.*/
    class SLIDEIO_ND2_EXPORTS ND2Scene : public CVScene
    {
    public:
        /// `position` is the ordinal of the scene's XY point among the valid points:
        /// the `p` of nd2::Experiment::frameIndex, and the scene index.
        ND2Scene(const std::string& filePath, int position, std::shared_ptr<const nd2::ImageInfo> info,
                 std::shared_ptr<const nd2::FrameSource> frames, Compression compression,
                 ResampleLimits limits);
        std::string getFilePath() const override { return m_filePath; }
        int getSceneIndex() const override { return m_position; }
        const std::string& getDriverId() const override { return m_driverId; }
        std::string getName() const override { return m_name; }
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        int getNumZSlices() const override { return m_numZ; }
        int getNumTFrames() const override { return m_numT; }
        DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        int getChannelSignificantBits(int channel) const override;
        bool hasPlaneTimestamps() const override;
        double getPlaneTimestamp(int tFrame, int channel, int zSlice) const override;
        int64_t getAcquisitionTime() const override;
        double getMagnification() const override;
        Compression getCompression() const override { return m_compression; }
        bool supportsConcurrentReads() const override { return true; }
        /// Fluorescence background is dark: 0 for every channel and type.
        double getChannelBackground(int channel) const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
    private:
        void checkChannel(int channel) const;
        // Design 6.3, 6.4: the time step and timestamps, worked out on first use, since
        // they read the times of the scene's frames (PR review: not at open).
        void initTimes() const;
        // Reads `rect` (inside the frame) of `frame` at native size, one plane per
        // entry of `channels`; zeros when the frame is absent. Through `cursor` when
        // one is given (the tiles of one tiled read), else straight from the source.
        void readRaw(const cv::Rect& rect, const std::vector<int>& channels, int64_t frame,
            nd2::FrameCursor* cursor, cv::Mat& out) const;
        int cvDepth() const;
    private:
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        int m_position;
        std::shared_ptr<const nd2::ImageInfo> m_info;
        std::shared_ptr<const nd2::FrameSource> m_frames;
        Compression m_compression;
        ResampleLimits m_limits;
        int m_numZ = 1;
        int m_numT = 1;
        // Set once by initTimes.
        mutable std::once_flag m_timesOnce;
        // Seconds: the measured step at this scene's position, else the nominal period.
        mutable double m_tStep = 0;
        mutable bool m_hasTimestamps = false;
        // dTimeAbsolute is known (>= 1, design 6.4).
        mutable bool m_hasAbsoluteTime = false;
        mutable int64_t m_acquisitionTime = 0;
        // Seconds from m_acquisitionTime to the scene's earliest frame, in [0, 1)
        // with an absolute time, 0 without.
        mutable double m_firstOffset = 0;
        // The scene's earliest frame time, ms.
        mutable double m_earliestMs = 0;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
