// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    // One libtiff handle of the scene's file, borrowed from the scene's pool for
    // exactly one read. Same shape as SVSReadContext.
    class GDALReadContext : public ReadContext
    {
    public:
        explicit GDALReadContext(const std::string& filePath);
        TIFFKeeper keeper;
    };

    // A scene of a generic tiff: a base directory and the reduced-resolution
    // directories groupTiffDirectories accepted as its zoom levels. Everything the
    // scene reports comes from copies of those directories; the only handles it
    // ever holds live in its ContextPool.
    class SLIDEIO_GDAL_EXPORTS GDALTiffScene : public CVScene, public Tiler
    {
    public:
        // levels[0] is the base; the rest descend in width.
        GDALTiffScene(const std::string& filePath, const std::string& driverId,
                      std::vector<TiffDirectory> levels);
        std::string getFilePath() const override { return m_filePath; }
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override { return m_driverId; }
        int getNumChannels() const override;
        DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override { return 0; }
        std::string getName() const override { return std::string(); }
        cv::Rect getRect() const override;
        Compression getCompression() const override;
        MetadataFormat getMetadataFormat() const override { return MetadataFormat::JSON; }
        std::string getRawMetadata() const override { return m_metadata; }
        // TIFFTAG_DATETIME of the base, in seconds since the Unix epoch; 0 where
        // the file states none. See TECH_DEBT #28.
        int64_t getAcquisitionTime() const override { return m_acquisitionTime; }
        ColorProfile getColorProfile() const override { return m_colorProfile; }
        // Each read borrows its own GDALReadContext (own libtiff handle) from the pool.
        bool supportsConcurrentReads() const override { return true; }
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        // Tiler methods. userData is the {directory, context} of one level read.
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
            cv::OutputArray output) override;
    private:
        // Reads a rect lying inside the level: one borrow, then tile by tile for a
        // tiled level or the whole level decoded for a striped one.
        void readLevelRect(int level, const cv::Rect& levelRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        int findZoomLevelIndex(double zoom) const;
    private:
        std::string m_filePath;
        std::string m_driverId;
        std::vector<TiffDirectory> m_directories;
        std::string m_metadata;
        int64_t m_acquisitionTime = 0;
        ColorProfile m_colorProfile;
        // Declared last on purpose, and it must stay last: ~ContextPool blocks until
        // every outstanding borrow is returned, and members die in reverse order, so
        // the pool goes before the directories an in-flight read's userData points into.
        ContextPool m_contextPool;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
