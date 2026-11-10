// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/bif/bif_api_def.hpp"
#include "slideio/drivers/bif/bifgeometry.hpp"
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief one scanned AOI of a BIF slide, stitched from its camera tiles.
     *
     * Its properties follow design §4.6: size from the AOI geometry at level 0, one
     * zoom level per pyramid directory, resolution and magnification from the iScan
     * acquisition data, the ICC profile of the level-0 directory, and the AOI's own
     * ImageInfo element (without its joints) as raw metadata.*/
    class SLIDEIO_BIF_EXPORTS BIFScene : public CVScene
    {
    public:
        // `levels` are the pyramid directories in level order, already validated by
        // BIFSlide: tiled, one tile size, integral downsamples.
        BIFScene(const std::string& path, const std::string& name, int sceneIndex,
                 std::vector<TiffDirectory> levels, bif::AoiGeometry geometry, bif::Aoi aoi,
                 bif::AcquisitionInfo acq);
        std::string getFilePath() const override { return m_filePath; }
        int getSceneIndex() const override { return m_sceneIndex; }
        const std::string& getDriverId() const override { return m_driverId; }
        std::string getName() const override { return m_name; }
        cv::Rect getRect() const override { return m_rect; }
        int getNumChannels() const override { return 3; }
        DataType getChannelDataType(int) const override { return DataType::DT_Byte; }
        Resolution getResolution() const override { return m_resolution; }
        double getMagnification() const override { return m_magnification; }
        Compression getCompression() const override { return m_compression; }
        ColorProfile getColorProfile() const override { return m_colorProfile; }
        bool supportsConcurrentReads() const override { return true; }
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
    private:
        // Stitches `rect`, which lies inside level `level`, at native size: each output
        // pixel is copied from the storage pixel the geometry maps it to (design §5.5),
        // and each storage tile the maps reference is decoded once.
        void renderLevelRect(int level, const cv::Rect& rect, const std::vector<int>& channels,
                             cv::Mat& out, ContextPool::Borrow& ctx) const;
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        int m_sceneIndex;
        std::vector<TiffDirectory> m_levelDirectories;
        std::vector<int> m_downsamples;
        bif::AoiGeometry m_geometry;
        bif::Aoi m_aoi;
        bif::AcquisitionInfo m_acquisition;
        cv::Rect m_rect;
        Resolution m_resolution;
        double m_magnification = 0.;
        Compression m_compression = Compression::Unknown;
        ColorProfile m_colorProfile;
        // Declared last on purpose, and it must stay last: ~ContextPool blocks until
        // every borrow is returned, and members die in reverse declaration order, so the
        // pool has to be destroyed before the directories and geometry an in-flight read uses.
        ContextPool m_contextPool;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
