// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/drivers/ome-tiff/otstructs.hpp"
#include "slideio/drivers/ome-tiff/tiffdata.hpp"
#include "slideio/imagetools/tifffiles.hpp"
#include "slideio/drivers/ome-tiff/otdimensions.hpp"
#include <tinyxml2.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace ometiff
    {
        struct ImageData;

        /// Per-thread libtiff handles for one OME-TIFF read.
        ///
        /// A collection rather than a single handle, because one tile read can
        /// span several TiffData that name different files -- the file comes
        /// from each <TiffData> element's UUID/FileName attribute. TIFFFiles
        /// opens lazily, so a context only ever holds handles to the files the
        /// thread it served actually read.
        ///
        /// Unlike the sibling drivers' contexts, this constructor opens nothing:
        /// TIFFFiles::getOrOpen raises on a failed TIFFOpen rather than
        /// returning null, so no call site needs a null check either way.
        class OTReadContext : public ReadContext
        {
        public:
            TIFFFiles files;
        };

        class SLIDEIO_OMETIFF_EXPORTS OTScene : public CVScene, public Tiler
        {
        public:
            explicit OTScene(const ImageData& filePath, int sceneIndex, const std::string& driverId);
            int getNumChannels() const override;
            cv::Rect getRect() const override;
            int findZoomLevel(double zoom) const;
            // Tiler methods
            int getTileCount(void* userData) override;
            bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
            bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                          void* userData) override;
            void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
            std::string getChannelName(int channel) const override;
            void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                cv::OutputArray output) override;
            void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
                const cv::Size& blockSize, const std::vector<int>& channelIndices,
                int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
            std::string getFilePath() const override;
			int getSceneIndex() const override { return m_sceneIndex; }
            const std::string& getDriverId() const override {
                return m_driverId;
            }
            std::string getName() const override;
            DataType getChannelDataType(int channel) const override;
            Resolution getResolution() const override;
            double getMagnification() const override;
            Compression getCompression() const override;
            int getNumZSlices() const override;
            int getNumTFrames() const override;
			/// The number of distinct files this scene's TiffData elements
			/// reference. Deterministic and independent of read history --
			/// deliberately not "handles currently open", which is now
			/// per-context and would read 0 before the first read.
			int getNumTiffFiles() const;
			int getNumTiffDataItems() const { return static_cast<int>(m_tiffData.size()); }
            const TiffData& getTiffData(int index) const { return m_tiffData[index]; }
            double getZSliceResolution() const override { return m_zResolution; }
            double getTFrameResolution() const override { return m_tResolution; }
        private:
            void extractImagePyramids();
            void initialize();
            void initializeChannelAttributes(tinyxml2::XMLElement* pixels);
            void extractMagnificationFromMetadata();
            void extractTiffData(tinyxml2::XMLElement* pixels, TIFFFiles& files);
            void extractImageIndex();
            LevelInfo extractLevelInfo(const TiffDirectory& dir, int index) const;
            void collectTiffDataIndices(std::vector<int> channelIndices, int zSliceIndex, int tFrameIndex,
                std::vector<int>& tiffDataIndices) const;
        protected:
            /// Borrows this scene's per-thread handles for the duration of one
            /// block read. Acquire once per read at the outermost entry point
            /// and pass the context down through BlockInfo; never re-acquire
            /// inside a read.
            ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
        private:
            int m_numChannels = 0;
            std::vector<std::string> m_channelNames;
            tinyxml2::XMLElement* m_imageXml;
            std::shared_ptr<tinyxml2::XMLDocument> m_imageDoc;
            std::string m_imageId;
			std::vector<TiffData> m_tiffData;
            std::string m_dimensionOrder;
            DataType m_dataType = DataType::DT_Unknown;
            int m_numZSlices = 0;
            int m_numTFrames = 0;
			cv::Size m_imageSize;
            bool m_bigEndian = false;
            std::string m_imageName;
            std::string m_filePath;
            Compression m_compression = Compression::Unknown;
            Resolution m_resolution = {};
            double m_magnification = 0;
            int m_imageIndex = -1;
			double m_zResolution = 0.0;
			double m_tResolution = 0.0;
            int m_sceneIndex = -1;
            std::string m_driverId;
            // Declared LAST deliberately. ~ContextPool blocks until every
            // outstanding Borrow is returned, and members are destroyed in
            // reverse declaration order -- so the pool must be declared after
            // m_tiffData, whose TiffData objects an in-flight read is reading.
            // Moving this line up reintroduces a use-after-free on close.
            ContextPool m_contextPool;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
