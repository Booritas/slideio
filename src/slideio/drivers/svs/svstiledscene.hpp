// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svsscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/tilecomposer.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_SVS_EXPORTS SVSTiledScene : public SVSScene, public Tiler
    {
    public:
        // Constructs and initializes. A factory rather than a constructor call because
        // initialize() reads the image description through a virtual method, and a
        // virtual call made from a constructor does not reach a derived override.
        static std::shared_ptr<SVSTiledScene> create(const std::string& filePath,
            const std::string& driverId, const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs);
        static std::shared_ptr<SVSTiledScene> create(const std::string& filePath,
            const std::string& driverId, libtiff::TIFF* hFile, const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs);
        int getNumChannels() const override;
        cv::Rect getRect() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        // Returns the index of the level serving a zoom, not the directory: the level index
        // is what the level-addressed read path takes.
        int findZoomLevelIndex(double zoom) const;
        // Tiler methods
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
            void* userData) override;
    protected:
        // Protected rather than public: a caller constructing the scene directly, instead
        // of through create(), would skip initialize() and silently get a scene with zero
        // zoom levels and zero resolution. The factories are members, so they still reach
        // these, and so does a derived class's own constructor.
        SVSTiledScene(const std::string& filePath,
                      const std::string& driverId,
                      const std::string& name,
                      const std::vector<slideio::TiffDirectory>& dirs);
        SVSTiledScene(const std::string& filePath,
			const std::string& driverId,
            libtiff::TIFF* hFile,
            const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs);
        void initialize();
        // Reads the format specific fields — resolution, magnification, raw metadata —
        // out of the description of the base directory. Called by initialize().
        virtual void processImageDescription();
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
        std::vector<slideio::TiffDirectory> m_directories;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
