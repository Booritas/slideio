// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "vsifile.hpp"
#include "vsiscene.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        /// One libtiff handle. A fresh handle is cheap here because
        /// TiffTools::setCurrentDirectory positions with TIFFSetSubDirectory(offset),
        /// so it jumps straight to the right IFD with no directory walk and no
        /// re-parse of the pyramid -- a context duplicates the descriptor, not the
        /// parsed model.
        class VsiTiffReadContext : public ReadContext
        {
        public:
            explicit VsiTiffReadContext(const std::string& filePath)
                : keeper(TiffTools::openTiffFile(filePath)) {
                if (!keeper.isValid()) {
                    RAISE_RUNTIME_ERROR << "VSIImageDriver: cannot open file " << filePath;
                }
            }
            TIFFKeeper keeper;
        };

        class SLIDEIO_VSI_EXPORTS VsiFileScene : public VSIScene
        {
        public:
            VsiFileScene(const std::string& filePath, int sceneIndex, const std::string& driverId, std::shared_ptr<VSIFile>& vsiFile, int directoryIndex);
        public:
            bool supportsConcurrentReads() const override { return true; }
            void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
            int getTileCount(void* userData) override;
            bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
            bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                          void* userData) override;
        protected:
            void init();
            /// Borrows a handle for the duration of one block read. Acquire once per
            /// read and pass the context down through userData -- never re-acquire
            /// mid-read.
            ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
        protected:
            int m_directoryIndex;
        private:
            ContextPool m_contextPool;
        };
    }

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
