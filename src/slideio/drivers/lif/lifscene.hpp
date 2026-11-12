// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/lif/lif_api_def.hpp"
#include "slideio/drivers/lif/liflayout.hpp"
#include "slideio/drivers/lif/liftimestamps.hpp"
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/core/cvscene.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class FileReader;

    namespace lif
    {
        /// What reads have done, for tests: shared by every scene opened with the
        /// ReadLimits that carry it.
        struct ReadStats
        {
            /// The most source pixels one read of the file assembled into one buffer.
            std::atomic<int64_t> peakSourcePixels{0};
            /// The most output rows one tiled downscale held sums for at once.
            std::atomic<int64_t> peakSumRows{0};
            void noteSource(int64_t pixels);
            void noteSumRows(int64_t rows);
        };

        /// Bounds of a read. A downscaling read whose source rectangle holds more pixels
        /// than maxResampleSourcePixels is read in tiles of at most that many pixels and
        /// area-filtered tile by tile on the global sampling grid, so memory stays
        /// bounded on large mosaics and the result does not depend on the budget
        /// (spec D 7).
        struct ReadLimits
        {
            int64_t maxResampleSourcePixels = int64_t(16) << 20;
            /// Optional: when set, reads record into it.
            std::shared_ptr<ReadStats> stats;
        };
    }

    /**@brief one field of one LIF image (spec D 6, 7).
     *
     * Its only file state is the slide's FileReader, which is cursor-free, and the
     * layout is immutable, so reads may overlap. Both are shared, so a scene stays
     * readable after its slide is gone.*/
    class SLIDEIO_LIF_EXPORTS LIFScene : public CVScene
    {
    public:
        LIFScene(const std::string& filePath, const std::string& name, int sceneIndex,
                 std::shared_ptr<const FileReader> file, std::shared_ptr<const lif::Layout> layout,
                 uint64_t payloadOffset, int field, const lif::ImageDescription& image,
                 lif::ReadLimits limits);
        std::string getFilePath() const override { return m_filePath; }
        int getSceneIndex() const override { return m_sceneIndex; }
        const std::string& getDriverId() const override { return m_driverId; }
        std::string getName() const override { return m_name; }
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
        bool hasPlaneTimestamps() const override;
        double getPlaneTimestamp(int tFrame, int channel, int zSlice) const override;
        int64_t getAcquisitionTime() const override;
        double getMagnification() const override { return m_magnification; }
        Compression getCompression() const override { return Compression::Uncompressed; }
        bool supportsConcurrentReads() const override { return true; }
        /// Fluorescence background is dark: 0 for every channel and type.
        double getChannelBackground(int channel) const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
    private:
        void checkChannel(int channel) const;
        // Reads `rect` (inside the image) at native size into `out`, one plane per
        // entry of `channels` (scene channels, already resolved).
        void readRaw(const cv::Rect& rect, const std::vector<int>& channels, int z, int t, cv::Mat& out) const;
        void readScalarPlane(const cv::Rect& rect, int channel, int z, int t, cv::Mat& plane) const;
        void readInterleaved(const cv::Rect& rect, const std::vector<int>& channels, int z, int t, cv::Mat& out) const;
        // Downscales `rect` (inside the image) to `size`, no larger on either axis, with
        // the area filter, reading the source in tiles within m_limits (spec D 7).
        void readTiledDownscale(const cv::Rect& rect, const cv::Size& size, const std::vector<int>& channels,
                                int z, int t, cv::Mat& out) const;
        // The same, accumulating in W (OpenCV's accumulator for the depth).
        template <typename W>
        void readTiledDownscaleAs(const cv::Rect& rect, const cv::Size& size, const std::vector<int>& channels,
                                  int z, int t, cv::Mat& out) const;
        int cvDepth() const;
    private:
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        int m_sceneIndex;
        std::shared_ptr<const FileReader> m_file;
        std::shared_ptr<const lif::Layout> m_layout;
        uint64_t m_payloadOffset;
        int m_field;
        lif::PlaneTimes m_times;
        bool m_hasTimestamps = false;
        lif::Calibration m_calibration;
        double m_magnification = 0;
        std::vector<std::string> m_channelNames;
        lif::ReadLimits m_limits;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
