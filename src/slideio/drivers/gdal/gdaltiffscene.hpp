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
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    // One libtiff handle of the file, borrowed from the file's pool for exactly
    // one read. Same shape as SVSReadContext. A handle is not tied to a scene:
    // every read seeks to its own directory, so any scene of the file can use it.
    class GDALReadContext : public ReadContext
    {
    public:
        explicit GDALReadContext(const std::string& filePath);
        TIFFKeeper keeper;
    };

    // A scene of a generic tiff: a base directory and the reduced-resolution
    // directories groupTiffDirectories accepted as its zoom levels. Everything the
    // scene reports comes from copies of those directories; the only handles it
    // ever reads through live in the file's ContextPool, which every scene of the
    // file shares -- so a plain many-page tiff read page by page holds at most
    // ContextPool::defaultMax() handles, not one per page.
    class SLIDEIO_GDAL_EXPORTS GDALTiffScene : public CVScene, public Tiler
    {
    public:
        // levels[0] is the base; the rest descend in width. contextPool hands out
        // GDALReadContexts of filePath and is shared with the file's other scenes.
        GDALTiffScene(const std::string& filePath, const std::string& driverId,
                      std::vector<TiffDirectory> levels, std::shared_ptr<ContextPool> contextPool);
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
        // Each read borrows its own GDALReadContext (own libtiff handle) from the
        // file's pool.
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
        // For tests: the pool this scene borrows its handles from.
        const ContextPool* contextPool() const { return m_contextPool.get(); }
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
        // Shared by every scene of the file. Declared last on purpose, and it must
        // stay last: members die in reverse order, so this reference is dropped
        // before the directories a read's userData points into. Only the scene
        // holding the last reference destroys the pool, and only then does it block
        // in ~ContextPool until every outstanding borrow is returned; otherwise
        // dropping it returns at once, and borrows still out belong to reads of the
        // other scenes, whose directories are alive.
        std::shared_ptr<ContextPool> m_contextPool;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
