// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/czi/czi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/drivers/czi/czisubblock.hpp"
#include "slideio/drivers/czi/czistructs.hpp"
#include <map>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class CZISlide;
    class SLIDEIO_CZI_EXPORTS CZIScene : public CVScene, public Tiler
    {
    public:
        struct SceneParams
        {
            int illuminationIndex;
            int bAcquisitionIndex;
            int rotationIndex;
            int sceneIndex;
            int hPhaseIndex;
            int viewIndex;
        };
    private:
        struct Tile
        {
            Tile() { rect = { 0,0,0,0 }; }
            std::vector<int> blockIndices;
            cv::Rect rect;
        };
        typedef std::vector<Tile> Tiles;
        struct ZoomLevel
        {
            double zoom{};
            CZISubBlocks blocks;
            Tiles tiles;
        };
        struct ComponentInfo
        {
            std::string name;
            DataType dataType;
        };
        struct SceneChannelInfo
        {
            std::string name;
            int32_t pixelType;
            int32_t pixelSize;
            int32_t firstComponent;
            int32_t numComponents;
            DataType componentType;
        };
        struct TilerData
        {
            int zoomLevelIndex;
            int zSliceIndex;
            int tFrameIndex;
            double relativeZoom;
        };
    public:
        CZIScene();
        std::string getFilePath() const override;
		int getSceneIndex() const override { return m_sceneIndex; }
        const std::string& getDriverId() const override {
            return m_driverId;
        }
        bool supportsConcurrentReads() const override { return true; }
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        int getNumZSlices() const override;
        int getNumTFrames() const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        int getChannelSignificantBits(int channelIndex) const override;
        bool hasPlaneTimestamps() const override { return !m_planeTimestamps.empty(); }
        double getPlaneTimestamp(int tFrame, int channel, int zSlice) const override;
        int64_t getAcquisitionTime() const override { return m_acquisitionTime; }
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        std::string getName() const override;
        void init(uint64_t sceneId, SceneParams& sceneParams, const std::string& filePath, int sceneIndex, const std::string& driverId, const CZISubBlocks& blocks, CZISlide* slide, bool mainScene = true);
        // interface Tiler implementaton
        int getTileCount(void* userData) override;
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
        bool readTile(int tileIndex, const std::vector<int>& componentIndices, cv::OutputArray tileRaster,
                        void* userData) override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) override;
        Compression getCompression() const override{
            return m_compression;
        }
        void addAuxImage(const std::string& name, std::shared_ptr<CVScene> image);
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
        bool isMosaic() const { return m_bMosaic; }
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& componentIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
    private:
        void setMosaic(bool mosaic) { m_bMosaic = mosaic; }
        void setupComponents(const std::map<int, int>& channelPixelType);
        void setupComponentSignificantBits();
        void collectPlaneTimestamps(const CZISubBlocks& blocks);
        void setupComponentsAux(const std::map<int, int>& channelPixelType);
        void generateSceneName();
        void computeSceneRect();
        void computeSceneTiles();
        void compute4DParameters();
        void updateTileRects(ZoomLevel& value);
        void updateTileRects();
        const ZoomLevel& getBaseZoomLevel() const;
        void initZoomLevelInfo();
        int findBlockIndex(const Tile& tile, const CZISubBlocks& blocks, int channelIndex, int zSliceIndex, int tFrameIndex) const ;
        const Tile& getTile(const TilerData* tilerData, int tileIndex) const;
        const CZISubBlocks& getBlocks(const TilerData* tilerData) const;
        bool blockHasData(const CZISubBlock& block, const std::vector<int>& componentIndices, const TilerData* tilerData);
        std::vector<uint8_t> decodeData(const CZISubBlock& block, const std::vector<unsigned char>& encodedData);
        void unpackChannels(const CZISubBlock& block, const std::vector<int>& orgComponentIndices, const std::vector<unsigned char>& blockData, const TilerData* tilerData, std::vector<cv::Mat>& componentRasters);
        void computeSceneMetadata();
    public:
        // static members
        static uint64_t sceneIdFromDims(int s, int i, int v, int h, int r, int b);
        static uint64_t sceneIdFromDims(const std::vector<Dimension>& dims);
        static void sceneIdsFromDims(const std::vector<Dimension>& dims, std::vector<uint64_t>& ids);
        static uint64_t sceneIdFromDims(const SceneParams& params);
        static void dimsFromSceneId(uint64_t sceneId, int& s, int& i, int& v, int& h, int& r, int& b);
        static void dimsFromSceneId(uint64_t sceneId, SceneParams& params);
        static void channelComponentInfo(CZIDataType channelType, DataType& componentType, int& numComponents, int& pixelSize);
    private:
        void combineBlockInTiles(ZoomLevel& zoomLevel);
        // data members
    private:
        std::vector<ZoomLevel> m_zoomLevels;
        std::vector<ComponentInfo> m_componentInfos;
        // ComponentBitCount of the CZI channel each scene component belongs to,
        // falling back to the image level count and 0 where the file states
        // neither. A channel of several components -- an interleaved Bgr24, say --
        // gives all of them its own count.
        std::vector<int> m_componentSignificantBits;
        // Seconds from getAcquisitionTime(), one per plane, indexed
        // (t * numZ + z) * numChannels + channel. Empty unless every plane states
        // a time. Channel here is the CZI channel, which an interleaved pixel
        // format expands into several scene components sharing one plane. A plane
        // built of many tiles takes the earliest time its tiles state.
        std::vector<double> m_planeTimestamps;
        int64_t m_acquisitionTime = 0;
        // Whether this scene has an acquisition origin of its own. Not the same as
        // a zero m_acquisitionTime, and not the same as the slide having one: an
        // attachment scene is excluded even where the main image states one.
        bool m_hasAcquisitionTime = false;
        int m_timestampChannels = 0;
        std::vector<SceneChannelInfo> m_channelInfos;
        std::string m_filePath;
        cv::Rect m_sceneRect;
        std::map<int, std::pair<int, int>> m_componentToChannelIndex;
        // Valid during init() only. A scene may outlive its slide, so what it needs from
        // the slide afterwards is copied into the members below, and the file is reached
        // through the shared reader.
        CZISlide* m_slide;
        std::shared_ptr<const FileReader> m_reader;
        Resolution m_resolution;
        double m_magnification = 0.;
        double m_zSliceResolution = 0.;
        double m_tFrameResolution = 0.;
        std::string m_name;
        uint64_t m_id{};
        SceneParams m_sceneParams{};
        int m_numZSlices;
        int m_numTFrames;
        int m_firstSliceIndex = 0;
        int m_firstTFrameIndex = 0;
        Compression m_compression;
        std::map<std::string, std::shared_ptr<CVScene>> m_auxImages;
        bool m_bMosaic;
        int m_sceneIndex;
        std::string m_driverId;

    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
