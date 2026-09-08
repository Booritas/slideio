// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <string>
#include <vector>

#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/drivers/vsi/etsfilescene.hpp"
#include "slideio/drivers/vsi/volume.hpp"
#include "slideio/drivers/vsi/vsistruct.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/drivers/vsi/vsistream.hpp"
#include "slideio/drivers/vsi/pyramid.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/readcontext.hpp"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        /// Scratch memory for one tile decode. Context-owned rather than
        /// thread_local: the buffer then dies with the EtsFile's pool instead of
        /// living for the thread's lifetime, and it is per-scene rather than
        /// per-process.
        ///
        /// The memory cost is worth stating, because the pool is kUnbounded and
        /// so never reclaims a context: `buffer` is resized up to each tile's
        /// compressed size and never shrinks, so an EtsFile ends up holding
        /// roughly (peak concurrent readers) x (largest compressed tile) bytes
        /// for as long as it lives. Peak concurrency is a high-water mark, not a
        /// current count -- a burst of 16 threads leaves 16 contexts behind. For
        /// tiles of tens to hundreds of kilobytes that is a few megabytes, which
        /// is why it is documented rather than bounded; a shrink_to_fit here
        /// would give back the memory and reintroduce a per-tile allocation.
        class EtsReadContext : public ReadContext
        {
        public:
            std::vector<uint8_t> buffer;
        };

        class SLIDEIO_VSI_EXPORTS EtsFile
        {
        public:
            EtsFile(const std::string& filePath);

            std::string getFilePath() const {
                return m_filePath;
            }

            DataType getDataType() const {
                return m_dataType;
            }

            int getNumChannels() const {
                return m_numChannels;
            }

            slideio::Compression getCompression() const {
                return m_compression;
            }

            void read(std::list<std::shared_ptr<Volume>>& volumes, TileInfoListPtr& tiles);
            void readTilePart(const vsi::TileInfo& tileInfo, EtsReadContext& context, cv::OutputArray tileRaster) const;
            bool assignVolume(std::list<std::shared_ptr<vsi::Volume>>& volumes);
            void initStruct(TileInfoListPtr& tiles);

            void setVolume(const std::shared_ptr<Volume>& volume) {
                m_volume = volume;
            }

            std::shared_ptr<Volume> getVolume() const {
                return m_volume;
            }

            const cv::Size& getSize() const {
                return m_size;
            }

            const cv::Size& getTileSize() const {
                return m_tileSize;
            }

            int getNumZSlices() const {
                return m_numZSlices;
            }

            int getNumTFrames() const {
                return m_numTFrames;
            }

            int getNumLambdas() const {
                return m_numLambdas;
            }

            int getNumPyramidLevels() const {
                return m_pyramid.getNumLevels();
            }

            const PyramidLevel& getPyramidLevel(int index) const {
                return m_pyramid.getLevel(index);
            }
            const cv::Size& getSizeWithCompleteTiles() const {
                return m_sizeWithCompleteTiles;
            }
            void readTile(int levelIndex, int tileIndex, const std::vector<int>& channelIndices, int zSlice, int tFrame,
                          EtsReadContext& context, cv::OutputArray output) const;
            /// Borrows scratch memory for the duration of one tile read. The pool is
            /// unbounded: the context holds no scarce resource -- only a scratch
            /// buffer -- so capping it would serialise a path with no contention.
            /// Acquire once per read (EtsFileScene::readResampledLevelBlockChannelsEx)
            /// and pass the context down through userData -- never re-acquire mid-read.
            ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
        private:
            std::string m_filePath;
            DataType m_dataType = DataType::DT_Unknown;
            int m_numChannels = 1;
            ColorSpace m_colorSpace = ColorSpace::Unknown;
            slideio::Compression m_compression = slideio::Compression::Unknown;
            int m_compressionQuality = 0;
            cv::Size m_size;
            cv::Size m_sizeWithCompleteTiles;
            cv::Size m_tileSize;
            int m_numZSlices = 1;
            int m_numTFrames = 1;
            int m_numLambdas = 1;
            uint32_t m_pixelInfoHints[17] = {0};
            uint32_t m_backgroundColor[10] = {0};
            bool m_usePyramid = false;
            int m_numDimensions;
            std::shared_ptr<Volume> m_volume;
            Pyramid m_pyramid;
            std::shared_ptr<const FileReader> m_reader;
            ContextPool m_contextPool;
            std::vector<int> m_maxCoordinates;
        };
    }
}
